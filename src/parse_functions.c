#include <stdlib.h>
#include <string.h>

#include "parse_functions.h"
#include "selint_error.h"
#include "tree.h"

char *module_name = NULL;

enum selint_error begin_parsing_te(struct policy_node **cur, char *mn) {

	set_current_module_name(mn);

	*cur = malloc(sizeof(struct policy_node));
	if (!*cur) {
		return SELINT_OUT_OF_MEM;
	}

	memset(*cur, 0, sizeof(struct policy_node));

	(*cur)->flavor = NODE_TE_FILE;
	(*cur)->data = strdup(mn);

	return SELINT_SUCCESS;
}

void set_current_module_name(char *mn) {
	if (module_name != NULL) {
		free(module_name);
	}
	module_name = strdup(mn);
}

char * get_current_module_name() {
	return module_name;
}

char *get_name_if_in_template(struct policy_node *cur) {
	while (cur->parent) {
		cur = cur->parent;
		if (cur->flavor == NODE_TEMP_DEF) {
			return cur->data;
		}
	}
	return NULL;
}

int is_in_require(struct policy_node *cur) {
	while (cur->parent) {
		cur = cur->parent;
		if (cur->flavor == NODE_GEN_REQ) {
			return 1;
		}
	}
	return 0;
};

enum selint_error insert_declaration(struct policy_node **cur, char *flavor, char *name) {
	//TODO: Handle attributes

	enum decl_flavor flavor_to_set = DECL_TYPE; // TODO: Other flavors

	if (!is_in_require(*cur)) {
		// In a require block, the objects arent being declared
		// Otherwise, we need to insert them into the appropriate map


		char *temp_name = get_name_if_in_template(*cur);

		if (temp_name) {
			// We are inside a template, so we need to save declarations in the template map
			// TODO: What about nested templates?  This case may require some thought
			insert_into_template_map(temp_name, flavor_to_set, name);
		} else {

			char *mn = get_current_module_name();

			if (!mn) {
				return SELINT_NO_MOD_NAME;
			}

			insert_into_type_map(name, mn);

		}
	}

	struct declaration_data *data = (struct declaration_data *) malloc(sizeof(struct declaration_data));
	if (!data) {
		return SELINT_OUT_OF_MEM;
	}

	memset(data, 0, sizeof(struct declaration_data));

	data->flavor = flavor_to_set;
	data->name = strdup(name);

	enum selint_error ret = insert_policy_node_next(*cur, NODE_DECL, data);

	if (ret != SELINT_SUCCESS) {
		free(data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_av_rule(struct policy_node **cur, enum av_rule_flavor flavor, struct string_list *sources, struct string_list *targets, struct string_list *object_classes, struct string_list *perms) {

	struct av_rule_data *av_data = malloc(sizeof(struct av_rule_data));

	av_data->flavor = flavor;
	av_data->sources = sources;
	av_data->targets = targets;
	av_data->object_classes = object_classes;
	av_data->perms = perms;

	enum selint_error ret = insert_policy_node_next(*cur, NODE_AV_RULE, av_data);
	if ( ret != SELINT_SUCCESS) {
		free_av_rule_data(av_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_type_transition(struct policy_node **cur, struct string_list *sources, struct string_list *targets, struct string_list *object_classes, char *default_type, char *name) {

	struct type_transition_data *tt_data = malloc(sizeof(struct type_transition_data));

	tt_data->sources = sources;
	tt_data->targets = targets;
	tt_data->object_classes = object_classes;
	tt_data->default_type = strdup(default_type);
	tt_data->name = strdup(name);

	enum selint_error ret = insert_policy_node_next(*cur, NODE_TT_RULE, tt_data);
	if ( ret != SELINT_SUCCESS) {
		free_type_transition_data(tt_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_interface_call(struct policy_node **cur, char *if_name, struct string_list *args) {
	struct if_call_data *if_data = malloc(sizeof(struct if_call_data));
	if_data->name = strdup(if_name);
	if_data->args = args;

	enum selint_error ret = insert_policy_node_next(*cur, NODE_IF_CALL, if_data);
	if (ret != SELINT_SUCCESS) {
		free_if_call_data(if_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error begin_block(struct policy_node **cur, enum node_flavor block_type, void *data) {
	enum selint_error ret = insert_policy_node_next(*cur, block_type, data);

	if ( ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;

	ret = insert_policy_node_child(*cur, NODE_START_BLOCK, NULL);
	if ( ret != SELINT_SUCCESS) {
		*cur = (*cur)->prev;
		free_policy_node((*cur)->next);
		return ret;
	}

	*cur = (*cur)->first_child;

	return SELINT_SUCCESS;
}

enum selint_error end_block(struct policy_node **cur, enum node_flavor block_type) {

	if ((*cur)->parent == NULL || (*cur)->parent->flavor != block_type) {
		return SELINT_NOT_IN_BLOCK;
	}

	*cur = (*cur)->parent;
	return SELINT_SUCCESS;
}
	

enum selint_error begin_optional_policy(struct policy_node **cur) {

	return begin_block(cur, NODE_OPTIONAL_POLICY, (void *) NULL);
}

enum selint_error end_optional_policy(struct policy_node **cur) {

	return end_block(cur, NODE_OPTIONAL_POLICY);
}

enum selint_error begin_interface_def(struct policy_node **cur, enum node_flavor flavor, char *name) {

	switch(flavor) {
		case NODE_IF_DEF:
		case NODE_TEMP_DEF:
			break;
		default:
			return SELINT_BAD_ARG;
	}

	return begin_block(cur, flavor, (void *) strdup(name));
}

enum selint_error end_interface_def(struct policy_node **cur) {

	return end_block(cur, NODE_IF_DEF);
}

enum selint_error begin_gen_require(struct policy_node **cur) {

	return begin_block(cur, NODE_GEN_REQ, (void *) NULL);
}

enum selint_error end_gen_require(struct policy_node **cur) {

	return end_block(cur, NODE_GEN_REQ);
}

void cleanup_parsing() {
	if (module_name) {
		free(module_name);
		module_name = NULL;
	}

	free_all_maps();
}
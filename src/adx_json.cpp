
#include "adx_json.h"

using namespace std;

int adx_json_key_value(string json_buf, string key, string &value)
{
	int errcode = E_SUCCESS;

	json_t *json_root = NULL;
	json_parse_document(&json_root, json_buf.c_str());
	if (!json_root) errcode = E_INVALID_PARAM_JSON;

	if (json_root) {
		json_t *label = json_find_first_label(json_root, key.c_str());
		if (label && label->type == JSON_STRING
				&& label->child && label->child->text) {
			value = label->child->text;

		} else {
			errcode = E_INVALID_PARAM_JSON;
		}
	}

	if (json_root) json_free_value(&json_root);
	return errcode;
}



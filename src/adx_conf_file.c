
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "adx_conf_file.h"

#define ASCIILINESZ 1024

typedef struct {
	adx_list_t next;
	char *section;
	char *key;
	char *value;

} adx_conf_file_node_t;

int adx_conf_file_add(adx_conf_file_t *cf, char *section, char *key, char *value)
{
	adx_conf_file_node_t *node = je_malloc(sizeof(adx_conf_file_node_t));
	if (!node) return -1;

	int len = strlen(section);
	node->section = je_malloc(len + 1);
	if (!node->section) return -1;

	len = strlen(key);
	node->key = je_malloc(len + 1);
	if (!node->key) return -1;

	len = strlen(value);
	node->value = je_malloc(len + 1);
	if (!node->value) return -1;

	strcpy(node->section, section);
	strcpy(node->key, key);
	strcpy(node->value, value);
	adx_list_add(cf, &node->next);
	return 0;
}

int adx_conf_file_parse(adx_conf_file_t *cf, char *path)
{
	char line    [ASCIILINESZ] = {0};
	char section [ASCIILINESZ] = {0};
	char key     [ASCIILINESZ] = {0};
	char value   [ASCIILINESZ] = {0};

	FILE *fp = fopen(path, "r");
	if (!fp) return -1;

	while (fgets(line, 1024, fp)) {

		int len = strlen(line) - 1;
		if (len == 0 || len >= ASCIILINESZ) continue;
		if (line[0] == '#') continue;

		while ((len >= 0) && ((line[len]=='\n') || (isspace(line[len])))) {
			line[len] = 0 ;
			len-- ;
		}

		if (line[0]=='[' && line[len]==']') {
			sscanf(line, "[%[^]]", section);
		}

		if (section[0] && 
				(sscanf (line, "%[^=] = \"%[^\"]\"",	key, value) == 2
				 ||  sscanf (line, "%[^=] = '%[^\']'",	key, value) == 2
				 ||  sscanf (line, "%[^=] = %[^;#]",	key, value) == 2)) {
			if (adx_conf_file_add(cf, section, key, value)) {
				fclose(fp);
				return -1;
			}
		}
	}

	fclose(fp);
	return 0;
}

char *get_adx_conf_file_string(adx_conf_file_t *cf, const char *section, const char *key)
{

	adx_list_t *p = NULL;
	adx_list_for_each(p, cf) {
		adx_conf_file_node_t *node = (adx_conf_file_node_t *)p;
		if (strcmp(node->section, section) == 0 && strcmp(node->key, key) == 0)
			return node->value;
	}

	return NULL;
}

int get_adx_conf_file_number(adx_conf_file_t *cf, const char *section, const char *key)
{
	char *value = get_adx_conf_file_string(cf, section, key);
	return value ? atoi(value) : 0;
}

adx_conf_file_t *adx_conf_file_load(char *path)
{
	adx_conf_file_t *cf = je_malloc(sizeof(adx_conf_file_t));
	if (!cf) return NULL;

	adx_list_init(cf);
	if (adx_conf_file_parse(cf, path) != 0) {
		adx_conf_file_free(cf);
		return NULL;
	}

	return cf;
}

void adx_conf_file_free()
{

	// TODO: 释放内存
}




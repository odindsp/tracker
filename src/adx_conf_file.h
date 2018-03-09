
#ifndef __ADX_CONF_FILE_H__
#define __ADX_CONF_FILE_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "adx_type.h"
#include "adx_list.h"

#ifdef __JEMALLOC__
#include <jemalloc/jemalloc.h>
#else
#include <stdlib.h>
#define je_malloc malloc
#define je_free free
#endif

	typedef adx_list_t adx_conf_file_t;

	adx_conf_file_t *adx_conf_file_load(char *path);
	void adx_conf_file_free();

	char *get_adx_conf_file_string(adx_conf_file_t *cf, const char *section, const char *key);
	int get_adx_conf_file_number(adx_conf_file_t *cf, const char *section, const char *key);

#define GET_CONF_STR(o,s,k) get_adx_conf_file_string(o,s,k)
#define GET_CONF_NUM(o,s,k) get_adx_conf_file_number(o,s,k)

#ifdef __cplusplus
}
#endif

#endif



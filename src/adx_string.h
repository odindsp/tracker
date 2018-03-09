
#ifndef __ADX_STRING_H__
#define __ADX_STRING_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include <string.h>
#include "adx_type.h"
#include "adx_alloc.h"

#ifdef __JEMALLOC__
#include <jemalloc/jemalloc.h>
#else
#include <stdlib.h>
#define je_malloc malloc
#define je_free free
#endif

#define ADX_TO_STR(o) adx_str_init(o, strlen(o))

    typedef struct {
	char *str;
	int len;
    } adx_str_t;

    int adx_empty(adx_str_t str);
    adx_str_t adx_str_init(const char *s, int len);

    adx_str_t adx_strdup(adx_str_t str);
    void adx_str_free(adx_str_t str);

    char *adx_string_to_upper(char *buf);
    char *adx_string_to_lower(char *buf);

    char *adx_string_url_param_value(const char *url, const char *key, char *value);

    char *url_encode(char const *s, int len, int *new_length);
    int url_decode(char *str, int len);

    char *int_to_binary(adx_pool_t *pool, int num, int len);

    char *base64_encode(const char *src, char *dest);
    char *base64_decode(const char *src, char *dest);

#ifdef __cplusplus
}
#endif

#endif



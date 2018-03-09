
#include "adx_cache.h"
#include <stdarg.h>

/*******************/
/* adx_cache_value */
/*******************/

// value string
adx_cache_value_t adx_cache_value_string(const char *key)
{
    adx_cache_value_t value;
    value.type = key ? ADX_CACHE_TYPE_STRING : ADX_CACHE_TYPE_ERROR;
    value.string = (char *)key;
    return value;
}

// value number
adx_cache_value_t adx_cache_value_number(adx_size_t key)
{
    adx_cache_value_t value;
    value.type = ADX_CACHE_TYPE_NUMBER;
    value.number = key;
    return value;
}

// value null
adx_cache_value_t adx_cache_value_null()
{
    adx_cache_value_t value;
    value.type = ADX_CACHE_TYPE_NULL;
    value.number = 0;
    return value;
}

// value free
void adx_cache_value_free(adx_cache_value_t value)
{
    if (value.type == ADX_CACHE_TYPE_STRING && value.string) {
	je_free(value.string);
    }
}

// buf copy
char *adx_cache_buf_copy(char *src)
{
    if (!src) return NULL;

    int len = strlen(src);
    char *buf = (char *)je_malloc(len + 1);
    if (!buf) return NULL;

    memcpy(buf, src, len);
    buf[len] = 0;
    return buf;
}

/******************/
/*   adx_cache    */
/******************/

// cache create
adx_cache_t *adx_cache_create()
{
    adx_cache_t *cache = je_malloc(sizeof(adx_cache_t));
    if (!cache) return NULL;
    memset(cache, 0, sizeof(adx_cache_t));

    // 初始化下级节点
    adx_list_init(&cache->child_list);
    cache->child_tree_string = RB_ROOT;
    cache->child_tree_number = RB_ROOT;
    return cache;
}

// cache free
void adx_cache_free(adx_cache_t *cache)
{
    if (!cache) return;

    adx_list_t *p = NULL;
    adx_list_t *head = &cache->child_list;
    for (p = head->next; p != head; ) {

	adx_cache_t *node = adx_list_entry(p, adx_cache_t, next_list);
	p = p->next;

	adx_cache_free(node);	// 递归子节点
    }

    adx_cache_value_free(cache->key);	// 释放 key
    adx_cache_value_free(cache->value);	// 释放 value
    je_free(cache);				// 释放 cache
}

static void *__adx_cache_free(adx_cache_t *cache)
{
    adx_cache_free(cache);
    return NULL;
}

// cache find
adx_cache_t *adx_cache_find(adx_cache_t *cache, adx_cache_value_t key)
{
    if (!cache) return NULL;
    if (key.type == ADX_CACHE_TYPE_STRING) {

	adx_rbtree_node *node = adx_rbtree_string_find(&cache->child_tree_string, key.string);
	if (node) return rb_entry(node, adx_cache_t, next_tree_string);

    } else if (key.type == ADX_CACHE_TYPE_NUMBER) {

	adx_rbtree_node *node = adx_rbtree_number_find(&cache->child_tree_number, key.number);
	if (node) return rb_entry(node, adx_cache_t, next_tree_number);
    }

    return NULL;
}

// cache find args
adx_cache_t *adx_cache_find_args(adx_cache_t *cache, ...)
{
    va_list args;
    va_start(args, cache);

    do {

	adx_cache_value_t key = va_arg(args, adx_cache_value_t);
	switch(key.type) {
	    case ADX_CACHE_TYPE_STRING :
	    case ADX_CACHE_TYPE_NUMBER :
		cache = adx_cache_find(cache, key);
		break;

	    case ADX_CACHE_TYPE_NULL :
		va_end(args);
		return cache;

	    default :
		va_end(args);
		return NULL;
	}

    } while(cache);

    va_end(args);
    return NULL;
}

// cache add
adx_cache_t *adx_cache_add(adx_cache_t *cache, adx_cache_value_t key, adx_cache_value_t value)
{
    if (!cache) return NULL;
    adx_cache_t *node = adx_cache_find(cache, key);
    if (node) return node;

    node = adx_cache_create();
    if (!node) return NULL;

    switch(key.type) {

	case ADX_CACHE_TYPE_STRING: // add string
	    node->key.type = ADX_CACHE_TYPE_STRING;
	    node->key.string = adx_cache_buf_copy(key.string);
	    if (!node->key.string) return __adx_cache_free(node);
	    node->next_tree_string.string = node->key.string;
	    break;

	case ADX_CACHE_TYPE_NUMBER: // add number
	    node->key.type = ADX_CACHE_TYPE_NUMBER;
	    node->key.number = key.number;
	    node->next_tree_number.number = node->key.number;
	    break;

	default: return __adx_cache_free(node);
    }

    switch(value.type) {

	case ADX_CACHE_TYPE_STRING: // add string
	    node->value.type = ADX_CACHE_TYPE_STRING;
	    node->value.string = adx_cache_buf_copy(value.string);
	    if (!node->value.string) return __adx_cache_free(node);
	    break;

	case ADX_CACHE_TYPE_NUMBER: // add number
	    node->value.type = ADX_CACHE_TYPE_NUMBER;
	    node->value.number = value.number;
	    break;

	case ADX_CACHE_TYPE_NULL: // add null
	    node->value.type = ADX_CACHE_TYPE_NULL;
	    node->value.number = 0;
	    break;

	default: return __adx_cache_free(node);
    }

    node->parent = cache;
    cache->child_total++;
    adx_list_add(&cache->child_list, &node->next_list);

    if (node->key.type == ADX_CACHE_TYPE_STRING)
	adx_rbtree_string_add(&cache->child_tree_string, &node->next_tree_string);
    else if (node->key.type == ADX_CACHE_TYPE_NUMBER)
	adx_rbtree_number_add(&cache->child_tree_number, &node->next_tree_number);

    return node;
}

// value display
void adx_cache_value_display(adx_cache_value_t value)
{
    switch(value.type) {
	case ADX_CACHE_TYPE_STRING:
	    fprintf(stdout, "[%s]", value.string);
	    break;

	case ADX_CACHE_TYPE_NUMBER:
	    fprintf(stdout, "[%lu]", value.number);
	    break;

	case ADX_CACHE_TYPE_NULL:
	    // fprintf(stdout, "[null]");
	    break;
    }
}

// cache display
void adx_cache_display(adx_cache_t *cache)
{
    if (!cache) return;

    adx_list_t *p = NULL;
    adx_list_for_each(p, &cache->child_list) {

	adx_cache_t *node = adx_list_entry(p, adx_cache_t, next_list);
	adx_cache_t *parent = node->parent;
	while(parent && parent->parent) {
	    fprintf(stdout, "│    ");
	    parent = parent->parent;
	}

	fprintf(stdout, "├──");
	adx_cache_value_display(node->key);
	adx_cache_value_display(node->value);
	// fprintf(stdout, "[%d]", node->child_total);
	fprintf(stdout, "\n");

	adx_cache_display(node);
    }
}

// cache to string
char *adx_cache_to_string(adx_cache_t *cache)
{
    if (!cache || cache->value.type != ADX_CACHE_TYPE_STRING)
	return NULL;
    return cache->value.string;
}

// cache to number
adx_size_t adx_cache_to_number(adx_cache_t *cache)
{
    if (!cache || cache->value.type != ADX_CACHE_TYPE_NUMBER)
	return 0;
    return cache->value.number;
}

char *adx_cache_value_to_string(adx_cache_value_t value)
{
    if (value.type == ADX_CACHE_TYPE_STRING)
	return value.string;
    return NULL;
}

adx_size_t adx_cache_value_to_number(adx_cache_value_t value)
{
    if (value.type == ADX_CACHE_TYPE_NUMBER)
	return value.number;
    return 0;
}

adx_cache_t *adx_cache_find_str(adx_cache_t *cache, const char *format, ...)
{
#if 0
    va_list ap;
    int len = 128;
    char key[len];
start:	
    va_start(ap, format);
    int size = vsnprintf(key, len, format, ap);
    if (size >= len) {
	len = size + 1;
	goto start;
    }

    va_end(ap);
    return adx_cache_find(cache, adx_cache_value_string(key));

#endif

    int len = 1024;
    char *buf = NULL;
    va_list ap;
loop:	
    buf = je_malloc(len);
    if (!buf) return NULL;

    va_start(ap, format);
    int size = vsnprintf(buf, len, format, ap);
    if (size >= len) {
	len = size + 1;
	je_free(buf);
	goto loop;
    }
    
    adx_cache_t *ret = adx_cache_find(cache, adx_cache_value_string(buf));
    je_free(buf);
    va_end(ap);

    return ret;
}



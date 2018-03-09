
#ifndef __ADX_CACHE_H__
#define __ADX_CACHE_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "adx_type.h"
#include "adx_list.h"
#include "adx_rbtree.h"

#ifdef __JEMALLOC__
#include <jemalloc/jemalloc.h>
#else
#include <stdlib.h>
#define je_malloc malloc
#define je_free free
#endif

#define ADX_CACHE_TYPE_STRING	1
#define ADX_CACHE_TYPE_NUMBER	2
#define ADX_CACHE_TYPE_NULL	3
#define ADX_CACHE_TYPE_ERROR	4

#define CACHE_STR(o)	adx_cache_value_string(o)
#define CACHE_NUM(o)	adx_cache_value_number(o)
#define CACHE_NULL	adx_cache_value_null

    typedef struct {
	int type; // 1:str 2:int 3:null
	union {
	    char *string;
	    adx_size_t number;
	};

    } adx_cache_value_t;

    typedef struct adx_cache_t adx_cache_t;
    struct adx_cache_t {

	adx_cache_value_t       key;
	adx_cache_value_t       value;

	adx_cache_t             *parent;		// 父节点
	int                     child_total;		// 子节点数量

	adx_list_t 		child_list;		// 子节点根 -> next_list
	adx_rbtree_head         child_tree_string;	// 子节点根 -> next_tree_string
	adx_rbtree_head         child_tree_number;	// 子节点根 -> next_tree_number

	adx_list_t              next_list;		// 节点 list 	<- 父节点 child_list
	adx_rbtree_node         next_tree_string;	// 节点 tree string	<- 父节点 child_tree_string
	adx_rbtree_node         next_tree_number;	// 节点 tree number	<- 父节点 child_tree_number
    };

    /*** cache create/free ***/
    adx_cache_t *adx_cache_create();
    void adx_cache_free(adx_cache_t *cache);

    /*** cache find ***/
    adx_cache_t *adx_cache_find(adx_cache_t *cache, adx_cache_value_t key);
    adx_cache_t *adx_cache_find_args(adx_cache_t *cache, ...);

    /*** cache add ***/
    adx_cache_t *adx_cache_add(adx_cache_t *cache, adx_cache_value_t key, adx_cache_value_t value);

    /*** value type ***/
    adx_cache_value_t adx_cache_value_string(const char *key);
    adx_cache_value_t adx_cache_value_number(adx_size_t key);
    adx_cache_value_t adx_cache_value_null();

    /*** cache display ***/
    void adx_cache_value_display(adx_cache_value_t value);
    void adx_cache_display(adx_cache_t *cache);

    /*** cache to type ***/
    char *adx_cache_to_string(adx_cache_t *cache);
    adx_size_t adx_cache_to_number(adx_cache_t *cache);

    /*** cache_value to type ***/
    char *adx_cache_value_to_string(adx_cache_value_t value);
    adx_size_t adx_cache_value_to_number(adx_cache_value_t value);

    // new API
    // call adx_cache_find(adx_cache_value_string(str))
    adx_cache_t *adx_cache_find_str(adx_cache_t *cache, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif




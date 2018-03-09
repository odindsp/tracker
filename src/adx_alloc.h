
#ifndef __ADX_ALLOC_H__
#define __ADX_ALLOC_H__

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

	typedef struct {
		adx_list_t head;
	} adx_pool_t;

	adx_pool_t *adx_pool_create();
	void *adx_alloc(adx_pool_t *pool, size_t size);
	void adx_free(adx_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif



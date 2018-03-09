
#ifndef __ADX_HASH_H__
#define __ADX_HASH_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "adx_type.h"
#include "adx_alloc.h"

#ifdef __JEMALLOC__
#include <jemalloc/jemalloc.h>
#else
#include <stdlib.h>
#define je_malloc malloc
#define je_free free
#endif



#ifdef __cplusplus
}
#endif

#endif



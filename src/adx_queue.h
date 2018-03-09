
#ifndef __ADX_QUEUE_H__
#define __ADX_QUEUE_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "adx_alloc.h"
#include "adx_string.h"

	typedef struct {
		adx_list_t queue;
		adx_str_t str;
	} adx_queue_t;

	void adx_queue_push(adx_list_t *queue, adx_str_t str);
	void adx_queue_push_dup(adx_list_t *queue, adx_str_t str);

	adx_str_t adx_queue_pop(adx_list_t *queue);

#define adx_queue_init adx_list_init

#ifdef __cplusplus
}
#endif

#endif



#ifndef __ADX_FLUME_H__
#define __ADX_FLUME_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "adx_alloc.h"
#include "adx_string.h"
#include "adx_queue.h"

	/*****************/
	/* adx_flume API */
	/*****************/
	typedef struct {
		const char *host;
		int port, sockfd, err;
	} adx_flume_conn_t;

	adx_flume_conn_t *connadx_flume_conn(const char *host, int port);
	int adx_flume_network_send(adx_flume_conn_t *conn, const char *buf, int len);

	/************************/
	/* adx_flume 多线程 API */
	/************************/
	int adx_flume_init(const char *host, int port); // 不能多次初始化, 全局队列(包含断线重连)

	void adx_flume_queue_send(const char *buf, int len);
	void adx_flume_queue_send_vlist(const char *format, va_list ap);
	void adx_flume_queue_send_format(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif




#ifndef __ADX_KAFKA_H__
#define __ADX_KAFKA_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "adx_type.h"
#include <librdkafka/rdkafka.h>

#ifdef __JEMALLOC__
#include <jemalloc/jemalloc.h>
#else
#include <stdlib.h>
#define je_malloc malloc
#define je_free free
#endif
	typedef struct {
		rd_kafka_t *kafka_conn;
		rd_kafka_conf_t *kafka_conf;

		rd_kafka_topic_t *topic_conn;
		rd_kafka_topic_conf_t *topic_conf;

		char error[512];

	} adx_kafka_conn_t;

	adx_kafka_conn_t *adx_kafka_conn(char *brokers, char *topic);
	void adx_kafka_close(adx_kafka_conn_t *conn);

	void adx_kafka_send(adx_kafka_conn_t *conn, const char *buf, int size);
	void adx_kafka_send_r(adx_kafka_conn_t *conn, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif



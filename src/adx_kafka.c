
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include "adx_kafka.h"

void adx_kafka_dr(rd_kafka_t *rk, void *payload, size_t len, rd_kafka_resp_err_t err,  void *opaque, void *msg_opaque)
{
	// fprintf(stdout, "==>%d\n", err);
}

void adx_kafka_msg(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
	// fprintf(stdout, "==>%d\n", rkmessage->err);
}

void adx_kafka_error(rd_kafka_t *rk, int err, const char *reason, void *opaque)
{
	// fprintf(stdout, "==>[%d][%s]\n", err, reason);
}

void adx_kafka_log(const rd_kafka_t *rk, int level, const char *fac, const char *buf)
{
	// fprintf(stdout, "==>%s\n", fac);
}

adx_kafka_conn_t *adx_kafka_conn(char *brokers, char *topic)
{
	adx_kafka_conn_t *conn = je_malloc(sizeof(adx_kafka_conn_t));
	if (!conn) return NULL;

	rd_kafka_conf_t *kafka_conf = rd_kafka_conf_new();
	if (!kafka_conf) return NULL;

	rd_kafka_topic_conf_t *topic_conf = rd_kafka_topic_conf_new();
	if (!topic_conf) return NULL;

	// rd_kafka_conf_t :: conf
	// rd_kafka_conf_set_error_cb(conf, adx_kafka_error); // 错误回掉(用来判断是否发送成功)
	rd_kafka_conf_set_log_cb(kafka_conf, adx_kafka_log); // 日志回掉
	// rd_kafka_conf_set_dr_msg_cb(conf, adx_kafka_msg);
	// rd_kafka_conf_set_dr_cb(conf, adx_kafka_dr);

	// rd_kafka_topic_conf_t :: topic_conf
	// rd_kafka_topic_conf_set(topic_conf, "request.timeout.ms", "1", adx_kafka_conn.error, 512);
	// rd_kafka_topic_conf_set(topic_conf, "socket.timeout.ms", "100", adx_kafka_conn.error, 512);
	// rd_kafka_topic_conf_set(topic_conf, "message.timeout.ms", "100", adx_kafka_conn.error, 512);

	rd_kafka_t *kafka_conn = rd_kafka_new(RD_KAFKA_PRODUCER, kafka_conf, conn->error, 512);
	if (!kafka_conn) return NULL;

	int ret = rd_kafka_brokers_add(kafka_conn, brokers);
	if (ret < 1) return NULL;

	rd_kafka_topic_t *topic_conn = rd_kafka_topic_new(kafka_conn, topic, topic_conf);
	if (!topic_conn) return NULL;

	conn->kafka_conn = kafka_conn;
	conn->kafka_conf = kafka_conf;

	conn->topic_conn = topic_conn;
	conn->topic_conf = topic_conf;
	return conn;
}

void adx_kafka_close(adx_kafka_conn_t *conn)
{
	// TODO: 暂未实现
}

void adx_kafka_send(adx_kafka_conn_t *conn, const char *buf, int size)
{
	// 此函数是线程安全函数,非阻塞函数,把数据放入缓存队列
	// 如发送失败,等待服务正常继续发送,如果缓存队列满返回错误!
	// int rc = rd_kafka_produce(conn->topic_conn, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_BLOCK, buf, strlen(buf), NULL, 0, NULL);
	int rc = rd_kafka_produce(conn->topic_conn,
			RD_KAFKA_PARTITION_UA,
			RD_KAFKA_MSG_F_COPY,
			(void *)buf,
			size,
			NULL,
			0,
			NULL);
	if (rc != 0) {
		//TODO: 写入日志队列满(发送失败)
	}
}

void adx_kafka_send_r(adx_kafka_conn_t *conn, const char *format, ...)
{
	int len = 1024;
	char *buf = NULL;
start:	
	buf = je_malloc(len);
	if (!buf) return;

	va_list ap;
	va_start(ap, format);
	int size = vsnprintf(buf, len, format, ap);
	if (size >= len) {
		len = size + 1;
		je_free(buf);
		goto start;
	}

	if (size > 0) adx_kafka_send(conn, buf, size);
	je_free(buf);
	va_end(ap);
}

#if 0
void *demo(void *p)
{
	int i;
	for(i = 0; i < 100; i++){
		adx_kafka_send_r(p, "%d123%d%s%d8%s\n", 0, 4, "56", 7, "9");
	}

	pthread_exit(NULL);
}

int main()
{

	char *brokers = "localhost:9092";
	char *topic = "sun_test";
	adx_kafka_conn_t *conn = adx_kafka_conn(brokers, topic);

	int i;
	pthread_t tid;
	for(i = 0; i < 10; i++) 
		pthread_create(&tid, NULL, demo, conn);


	// adx_kafka_send_r(conn, "%d123%d%s%d8%s", 0, 4, "56", 7, "9");

	for(;;)sleep(1);
	return 0;
}

#endif




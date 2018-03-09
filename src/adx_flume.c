
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/wait.h>    
#include <pthread.h>
#include "adx_flume.h"

int adx_network_check(int sockfd)
{
	int error = -1;
	socklen_t len = sizeof(int);
	if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
		return -1;
	return error;
}

int adx_network_noblocking(int sockfd)
{
	int opts = -1;
	if ((opts = fcntl(sockfd, F_GETFD, 0)) == -1)
		return -1;
	if ((fcntl(sockfd, F_SETFL, opts | O_NONBLOCK)) == -1)
		return -1;
	return 0;
}

int adx_network_notwait(int sockfd)
{
	struct linger opt;
	opt.l_onoff = 0x01;
	opt.l_linger = 0x00;
	return setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &opt, sizeof(struct linger));
}

int adx_network_send_timeout(int sockfd, int t)
{
	struct timeval timeout = {0, t};
	return setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
}

int adx_network_recv_timeout(int sockfd, int t)
{
	struct timeval timeout = {0, t};
	return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
}

int adx_network_set_kernel_buffer(int sockfd, int send_size, int recv_size)
{
	int size;
	if ((size = send_size))
		if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(int)) == -1)
			return -1;

	if ((size = recv_size))
		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(int)) == -1)
			return -1;
	return 0;
}

int adx_network_get_kernel_buffer(int sockfd, int *send_size, int *recv_size)
{
	*send_size = -1;
	*recv_size = -1;

	socklen_t len = sizeof(int);
	if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, send_size, &len) == -1)
		return -1;

	if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, recv_size, &len) == -1)
		return -1;

	return 0;
}

int adx_network_connect(const char *host, int port)
{

	struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(host);
	addr.sin_port = htons(port);

	int sockfd = -1;
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		close(sockfd);
		return -1;
	}
#ifdef __NET_NOBLOCK__
	if (adx_network_noblocking(sockfd) == -1) 
		goto err;
#endif
	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
		return sockfd;

#ifdef __NET_NOBLOCK__
	if (errno == EINPROGRESS)
		return sockfd;
err:
#endif
	close(sockfd);
	return -1;
}

/*****************/
/* adx_flume API */
/*****************/
#define SEND_TIMEOUT 5000
#define KERNEL_SNDBUF 1024 * 1024 * 100
adx_flume_conn_t *connadx_flume_conn(const char *host, int port)
{

	int sockfd = adx_network_connect(host, port);
	if (!sockfd) return NULL;

	int ret = adx_network_send_timeout(sockfd, SEND_TIMEOUT);
	if (ret) goto err;

	ret = adx_network_set_kernel_buffer(sockfd, KERNEL_SNDBUF, 0);
	if (ret) goto err;

	adx_flume_conn_t *conn = je_malloc(sizeof(adx_flume_conn_t));
	if (ret) goto err;

	// int send_size, recv_size;
	// adx_network_get_kernel_buffer(sockfd, &send_size, &recv_size);
	// fprintf(stdout, "[KERNEL_SNDBUF=%d][SO_SNDBUF=%d][SO_RCVBUF=%d]\n", KERNEL_SNDBUF, send_size, recv_size);

	conn->host = host;
	conn->port = port;
	conn->sockfd = sockfd;
	signal(SIGPIPE, SIG_IGN);
	return conn;
err:
	close(sockfd);
	return NULL;
}

int adx_flume_send(adx_flume_conn_t *conn, const char *buf, int len)
{
	int size = write(conn->sockfd, buf, len);
	if (size == len) return 0;

	if (size > 0) { // KERNEL_SNDBUF 已满
		int send_size = size;
		for(;;) {
			size = write(conn->sockfd, &buf[send_size], len - send_size);
			if (send_size <= 0) break;

			send_size += size;
			if (send_size >= len) {
				return 0;
			}
		}
	}
#ifdef __NET_NOBLOCK__
	if (size == -1) {
		if (errno == EINTR || errno == EAGAIN)
			return 0;

		if (adx_network_check(conn->sockfd) == 0)
			return 0;
	}
#endif
	if (size == 0) goto err2;

	int sockfd = adx_network_connect(conn->host, conn->port);
	if (sockfd <= 0) goto err2;

	int ret = adx_network_set_kernel_buffer(sockfd, KERNEL_SNDBUF, 0);
	if (ret) goto err;

	ret = adx_network_send_timeout(sockfd, SEND_TIMEOUT);
	if (ret) goto err;

	close(conn->sockfd);
	conn->sockfd = sockfd;
	return adx_flume_send(conn, buf, len); // 重发
err:
	close(sockfd);
err2:
	close(conn->sockfd);
	conn->sockfd = -1;
	return -1;
}

/************************/
/* adx_flume 多线程 API */
/************************/

static adx_list_t adx_flume_queue = {&adx_flume_queue, &adx_flume_queue};
static pthread_mutex_t adx_flume_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static int adx_flume_init_code = 0;

// adx_flume 发送线程
void *adx_flume_send_thread(void *arg)
{
	pthread_detach(pthread_self());
	adx_flume_conn_t *conn = (adx_flume_conn_t *)arg;

	for(;;) {

		fprintf(stdout, "[flume_queue_wait]\n");

		pthread_mutex_lock(&adx_flume_queue_mutex);
		adx_str_t v = adx_queue_pop(&adx_flume_queue);
		pthread_mutex_unlock(&adx_flume_queue_mutex);

		fprintf(stdout, "[flume_queue_pop][%d][%d]%s\n", conn->sockfd, v.len, v.str);

		if (adx_empty(v)) {
			usleep(1);
			continue;
		}

		adx_flume_send(conn, v.str, v.len);
		adx_str_free(v);
	}

	pthread_exit(NULL);
}

// adx_flume 初始化发送线程
int adx_flume_init(const char *host, int port)
{
	adx_flume_conn_t *conn = connadx_flume_conn(host, port);
	if (!conn) {
		fprintf(stdout, "adx_flume_conn=ERR\n");
		adx_flume_init_code = -1;	
		return -1;
	}

	// 创建发送线程
	pthread_t tid;
	adx_flume_init_code = pthread_create(&tid, NULL, adx_flume_send_thread, conn);
	return adx_flume_init_code;
}

// adx_flume 发送queue
void adx_flume_queue_send(const char *buf, int len)
{
	adx_str_t v = {(char *)buf, len};
	if (adx_flume_init_code) return; // 未初始化

	pthread_mutex_lock(&adx_flume_queue_mutex);
	adx_queue_push(&adx_flume_queue, adx_strdup(v));
	pthread_mutex_unlock(&adx_flume_queue_mutex);
}

// adx_flume 发送queue
void adx_flume_queue_send_vlist(const char *format, va_list ap)
{
	int len = 1024;
	char *buf = NULL;
	va_list ap2;
start:	
	buf = je_malloc(len);
	if (!buf) return;

	va_copy(ap2, ap);
	int size = vsnprintf(buf, len, format, ap2);
	if (size + 1 >= len) {
		len = size + 2; // 多预留1位内存 存放\n
		je_free(buf);
		goto start;
	}

	if (size > 0) {
		buf[size++] = '\n'; // flume server 利用\n分隔报文 
		fprintf(stdout, "flume_queue=%s", buf);
		adx_flume_queue_send(buf, size);
	}

	je_free(buf);
	va_end(ap);
}

// adx_flume 发送 queue
void adx_flume_queue_send_format(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	adx_flume_queue_send_vlist(format, ap);
	va_end(ap);
}

#if 0
void *adx_flume_task_demo(void *arg)
{

	int i;
	for (i = 0; i < 10; i++) {
		adx_flume_queue_send_format("%05d", i);
	}

	return NULL;
}

int main()
{
	int errcode = adx_flume_init("127.0.0.1", 9093);
	fprintf(stdout, "errcode=%d\n", errcode);

	int i;
	pthread_t tid;
	for (i = 0; i < 10; i++) {
		pthread_create(&tid, NULL, adx_flume_task_demo, NULL);
	}

	sleep(1);
	return 0;
}

#endif




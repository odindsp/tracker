
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include "adx_queue.h"
#include "adx_log.h"

/**********************/
/* LOG CORE */
/**********************/
adx_log_t *adx_log_create(const char *dir_path, const char *name, int level)
{
	adx_log_t *log = je_malloc(sizeof(adx_log_t));
	if (!log) return NULL;

	log->level = level;
	log->dir_path = dir_path;
	log->name = name;

	time_t t = time(NULL);
	localtime_r(&t, &log->date);
	pthread_mutex_init(&log->mutex, NULL);
	log->fp = NULL;
	return log;
}

int adx_log_open(adx_log_t *log)
{
	char path[2048];
	sprintf(path, "%s/%s_%04d_%02d_%02d.log",
			log->dir_path,
			log->name,
			log->date.tm_year + 1900,
			log->date.tm_mon + 1,
			log->date.tm_mday);

	FILE *fp = fopen(path, "a+");
	if (!fp) return -1;

	if (log->fp) fclose(log->fp);
	log->fp = fp;
	return 0;
}

void adx_log_close(adx_log_t *log)
{
	if (log) fclose(log->fp);
}

void adx_log_write_buf(adx_log_t *log, int level, const char *buf, int len)
{

	if (!log || level < LOGINFO || level > LOGDEBUG || level < log->level) return;
	if (!log->dir_path || !log->name || !log->fp) return;

	struct tm tm;
	time_t t = time(NULL);
	localtime_r(&t, &tm);

	pthread_mutex_lock(&log->mutex); // lock

	if (log->date.tm_mday != tm.tm_mday) {
		log->date = tm;
		adx_log_open(log);
	}

	fprintf(log->fp, "[%04d-%02d-%02d %02d:%02d:%02d]",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec);

	fwrite(buf, len, 1, log->fp);
	fputc('\n', log->fp);
	fflush(log->fp);

	pthread_mutex_unlock(&log->mutex); // unlock

#ifdef __DEBUG__
	fwrite(buf, len, 1, stdout);
	fputc('\n', stdout);
	fflush(stdout);
#endif
}

void adx_log_write_vlist(adx_log_t *log, int level, const char *format, va_list ap)
{
	if (!log || level < LOGINFO || level > LOGDEBUG || level < log->level) return;
	if (!log->dir_path || !log->name || !log->fp) return;

	struct tm tm;
	time_t t = time(NULL);
	localtime_r(&t, &tm);

	pthread_mutex_lock(&log->mutex); // lock

	if (log->date.tm_mday != tm.tm_mday) {
		log->date = tm;
		adx_log_open(log);
	}

	fprintf(log->fp, "[%04d-%02d-%02d %02d:%02d:%02d]",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_hour,
			tm.tm_min,
			tm.tm_sec);
	vfprintf(log->fp, format, ap);
	fputc('\n', log->fp);
	fflush(log->fp);

	pthread_mutex_unlock(&log->mutex); // unlock
	va_end(ap);
}

void adx_log_write(adx_log_t *log, int level, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	adx_log_write_vlist(log, level, format, ap);
	va_end(ap);	
}

/**********************/
/* LOG QUEUE */
/**********************/
void *adx_log_queue_write_thread(void *arg)
{
	pthread_detach(pthread_self());
	adx_log_queue_t *log_queue = (adx_log_queue_t *)arg;
	adx_log_t *log = log_queue->log;

	for(;;) {

		sem_wait(&log_queue->sem); // 阻塞信号量

		for(;;) {

			pthread_mutex_lock(&log_queue->mutex);
			adx_str_t v = adx_queue_pop(&log_queue->queue);
			pthread_mutex_unlock(&log_queue->mutex);

			if (adx_empty(v)) break; // 队列已空

			adx_log_write_buf(log, log->level, v.str, v.len);
			adx_str_free(v);
		}
	}

	pthread_exit(NULL);
}

void adx_log_queue_write_vlist(adx_log_queue_t *log_queue, int level, const char *format, va_list ap)
{
	if (!log_queue) return;
	adx_log_t *log = log_queue->log;

	if (!log || level < LOGINFO || level > LOGDEBUG || level < log->level) return;
	if (!log->dir_path || !log->name || !log->fp) return;

	int len = 1024;
	char *buf = NULL;
	va_list ap2;
loop:	
	buf = je_malloc(len);
	if (!buf) return;

	va_copy(ap2, ap);
	int size = vsnprintf(buf, len, format, ap2);
	if (size >= len) {
		len = size + 1;
		je_free(buf);
		goto loop;
	}

	if (size > 0) {

		adx_str_t v = {buf, size};
		pthread_mutex_lock(&log_queue->mutex);
		adx_queue_push_dup(&log_queue->queue, v);
		pthread_mutex_unlock(&log_queue->mutex);

		sem_post(&log_queue->sem); // 发送信号量
	}

	je_free(buf);
	va_end(ap2);
}

void adx_log_queue_write(adx_log_queue_t *log_queue, int level, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	adx_log_queue_write_vlist(log_queue, level, format, ap);
	va_end(ap);
}

adx_log_queue_t *adx_log_queue_create(const char *dir_path, const char *name, int level)
{
	adx_log_t *log = adx_log_create(dir_path, name, level);
	if (!log) return NULL;

	int errcode = adx_log_open(log);
	if (errcode) goto err;

	adx_log_queue_t *log_queue = je_malloc(sizeof(adx_log_queue_t));
	if (!log_queue) goto err;

	log_queue->log = log;
	adx_list_init(&log_queue->queue);
	pthread_mutex_init(&log_queue->mutex, NULL);
	sem_init(&log_queue->sem, 0, 0);

	errcode = pthread_create(&log_queue->tid, NULL, adx_log_queue_write_thread, log_queue);
	if (errcode) goto err;

	return log_queue;
err:
	adx_log_close(log);
	if (log) je_free(log);
	if (log_queue) je_free(log_queue);
	return NULL;

}

/**********************/
/* 全局LOG (线程安全) */
/**********************/
static adx_log_queue_t *log_queue_global = NULL; // 队列全局指针
int adx_log_init(const char *dir_path, const char *name, int level)
{
	log_queue_global = adx_log_queue_create(dir_path, name, level);
	return log_queue_global ? 0 : -1;
}

void adx_log(int level, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	adx_log_queue_write_vlist(log_queue_global, level, format, ap);
	va_end(ap);
}


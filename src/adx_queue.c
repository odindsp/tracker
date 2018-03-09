
#include "adx_queue.h"

void adx_queue_push(adx_list_t *queue, adx_str_t str)
{
	if (adx_empty(str)) return;

	adx_queue_t *node = je_malloc(sizeof(adx_queue_t));
	if (!node) return;

	node->str = str;
	adx_list_add(queue, &node->queue);
}

void adx_queue_push_dup(adx_list_t *queue, adx_str_t str)
{
	return adx_queue_push(queue, adx_strdup(str));
}

adx_str_t adx_queue_pop(adx_list_t *queue)
{
	adx_str_t str = {0};
	adx_queue_t *node = (adx_queue_t *)adx_queue(queue);
	if (node) {
		str = node->str;
		je_free(node);
	}

	return str;
}

#if 0
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

sem_t sem;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	int index;
	adx_list_t *queue;
} th_info_t;

void *t1(void *p)
{
	th_info_t *info = (th_info_t *)p;
	// fprintf(stdout, "[T][%zd][%03d][%p]\n", pthread_self(), info->index, info->queue);

	sem_wait(&sem);

	int i;
	char buf[1024];
	for (i = 0; i < 10; i++) {

		sprintf(buf, "%03d-%03d", info->index, i);
		// fprintf(stdout, "%s\n", buf);

		adx_str_t v = {buf, 1024};
		pthread_mutex_lock(&queue_mutex);
		adx_queue_push(info->queue, adx_strdup(v));
		pthread_mutex_unlock(&queue_mutex);
	}

	pthread_exit(NULL);
}

void *t2(void *p)
{
	for(;;) {
		pthread_mutex_lock(&queue_mutex);
		adx_str_t v = adx_queue_pop(p);
		pthread_mutex_unlock(&queue_mutex);

		if (adx_empty(v)) {
			usleep(1);
			continue;
		}

		fprintf(stdout, "[%d][%s]\n", v.len, v.str);
		adx_str_free(v);
	}
}

int main()
{

	adx_list_t *queue = malloc(sizeof(adx_list_t));
	adx_queue_init(queue);
	sem_init(&sem, 0, 0);

	int i;
	pthread_t tid[100];
	th_info_t info[100];
	for (i = 0; i < 10; i++) {

		info[i].index = i;
		info[i].queue = queue;
		pthread_create(&tid[i], NULL, t1, &info[i]);
		// fprintf(stdout, "[M][%zd][%03d][%p]\n", tid[i], info[i].index, &queue);
	}

	pthread_t t2_tid;
	pthread_create(&t2_tid, NULL, t2, queue);

	sleep(1);
	for (i = 0; i < 100; i++) {
		sem_post(&sem);
	}

	sleep(1);
	// for(;;)sleep(1);
	return 0;
}

#endif





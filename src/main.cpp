
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "tracker.h"
#include "adx_alloc.h"
#include "adx_dump.h"
#include "adx_kafka.h"
#include "adx_flume.h"
#include "adx_conf_file.h"
#include "adx_log.h"
#include "adx_dsp_control.h"
#include "adx_time.h"

#define TRACKER_VERSION		"3.000"
#define	TRACKER_CONF_PATH	"/etc/dspads_odin/"
#define	TRACKER_CONF_FILE	"dsp_tracker.conf"
#define PRICE_CONF_FILE		"dsp_price.conf"

using namespace std;

bool run_flag = true;
bool get_run_flag(){return run_flag;}

void *adx_dsp_thread(void *arg)
{

    FCGX_Request request;
    FCGX_InitRequest(&request, 0, 0);

    while (run_flag) {
	adx_pool_t *pool = adx_pool_create();
	adx_dsp_doit(&request, pool, (adx_context_t *)arg);
	adx_free(pool);
#ifdef __DEBUG__
	run_flag = false;
	return 0;
#endif
    }

    pthread_exit(NULL);
}

#define IF_ADX_DSP_CONF_STR(o, s1, s2, s3) {o=GET_CONF_STR(s1,s2,s3);if(o==NULL){cout<<"conf err:"<<s3<<endl;return NULL;}}
#define IF_ADX_DSP_CONF_NUM(o, s1, s2, s3) {o=GET_CONF_NUM(s1,s2,s3);if(o==0){cout<<"conf err:"<<s3<<endl;return NULL;}}
adx_dsp_conf_t *adx_dsp_conf_load()
{

    adx_dsp_conf_t *conf = new adx_dsp_conf_t;
    if (!conf) return NULL;

    adx_conf_file_t *tracker_conf = adx_conf_file_load((char *)TRACKER_CONF_PATH TRACKER_CONF_FILE);
    if (!tracker_conf) return NULL;

    IF_ADX_DSP_CONF_NUM(conf->cpu_count, 		tracker_conf, "default", "cpu_count");
    IF_ADX_DSP_CONF_STR(conf->log_path, 		tracker_conf, "default", "log_path");
    IF_ADX_DSP_CONF_NUM(conf->log_level, 		tracker_conf, "default", "log_level");

    IF_ADX_DSP_CONF_STR(conf->AdSetting_ip, 	tracker_conf, "redis", "AdSetting_ip");
    IF_ADX_DSP_CONF_NUM(conf->AdSetting_port, 	tracker_conf, "redis", "AdSetting_port");
    // IF_ADX_DSP_CONF_STR(conf->AdSetting_pass, 	tracker_conf, "redis", "AdSetting_pass");
    conf->AdSetting_pass =             GET_CONF_STR(tracker_conf, "redis", "AdSetting_pass");

    IF_ADX_DSP_CONF_STR(conf->AdController_ip, 	tracker_conf, "redis", "AdController_ip");
    IF_ADX_DSP_CONF_NUM(conf->AdController_port, 	tracker_conf, "redis", "AdController_port");
    // IF_ADX_DSP_CONF_STR(conf->AdController_pass, tracker_conf, "redis", "AdController_pass");
    conf->AdController_pass =          GET_CONF_STR(tracker_conf, "redis", "AdController_pass");

    IF_ADX_DSP_CONF_STR(conf->AdCounter_ip, 	tracker_conf, "redis", "AdCounter_ip");
    IF_ADX_DSP_CONF_NUM(conf->AdCounter_port, 	tracker_conf, "redis", "AdCounter_port");
    // IF_ADX_DSP_CONF_STR(conf->AdCounter_pass, 	tracker_conf, "redis", "AdCounter_pass");
    conf->AdCounter_pass =             GET_CONF_STR(tracker_conf, "redis", "AdCounter_pass");

    IF_ADX_DSP_CONF_STR(conf->flume_ip, 		tracker_conf, "flumeserver", "flume_ip");
    IF_ADX_DSP_CONF_NUM(conf->flume_port, 		tracker_conf, "flumeserver", "flume_port");

    IF_ADX_DSP_CONF_STR(conf->kafka_broker, 	tracker_conf, "kafkaserver", "broker_list");
    IF_ADX_DSP_CONF_STR(conf->kafka_topic, 		tracker_conf, "kafkaserver", "topic");

    IF_ADX_DSP_CONF_STR(conf->link_jump_broker_list,	tracker_conf,	"kafkaserver",	"link_jump_broker_list");
    IF_ADX_DSP_CONF_STR(conf->link_jump_topic,	tracker_conf,	"kafkaserver",	"link_jump_topic");

    // IF_ADX_DSP_CONF_NUM(conf->price_upper, 		tracker_conf, "price", "upper"); // 价格预警

    IF_ADX_DSP_CONF_STR(conf->activation_url, 	tracker_conf, "activation", "activation_url");
    IF_ADX_DSP_CONF_STR(conf->activation_parameter, 	tracker_conf, "activation", "activation_parameter");

    return conf;
}

int redis_init(adx_dsp_conf_t *conf, adx_context_t *context_info)
{
    // redis 物料
    context_info->AdSetting_slave = redisConnect(conf->AdSetting_ip, conf->AdSetting_port, conf->AdSetting_pass);
    if (!context_info->AdSetting_slave || context_info->AdSetting_slave->err) {
	cout << "redis conn err : AdSetting_slave" << endl;
	return E_REDIS_CONNECT_INVALID;
    }

    // redis 投放控制
    context_info->AdController_master = redisConnect(conf->AdController_ip, conf->AdController_port, conf->AdController_pass);
    if (!context_info->AdController_master || context_info->AdController_master->err) {
	cout << "redis conn err : AdController_master" << endl;
	return E_REDIS_CONNECT_INVALID;
    }

    // redis集群 展现/点击/价格/匀速/KPI/频次等 
    context_info->AdCounter_master = redisClusterConnect(conf->AdCounter_ip, conf->AdCounter_port, conf->AdCounter_pass);
    if (!context_info->AdCounter_master || context_info->AdCounter_master->err) {
	cout << "redis conn err : AdCounter_master" << endl;
	return E_REDIS_CONNECT_INVALID;
    }

    return 0;
}

int adx_price_lib_path_init(adx_dsp_conf_t *conf, vector<adx_price_lib_path> &price_libs)
{
    adx_conf_file_t *conf_price = adx_conf_file_load((char *)TRACKER_CONF_PATH PRICE_CONF_FILE);
    if (!conf_price) return -1;

    for (int i = 1; i < 256; i++) {

	char index[128];
	sprintf(index, "%d", i);
	char *path = GET_CONF_STR(conf_price, "default", index);
	if (path) {

	    adx_price_lib_path node;
	    node.adx = i;
	    node.path = path;
	    node.dlopen_handle = dlopen(path, RTLD_LAZY);
	    if (!node.dlopen_handle) {
		cout << "price_lib err : " << path << endl;
		return -1;
	    }

	    price_libs.push_back(node);
	}
    }

    return 0;
}

adx_context_t *context_info_init(adx_dsp_conf_t *conf, adx_kafka_conn_t *kafka_conn, adx_kafka_conn_t *link_jump_conn, vector<adx_price_lib_path> price_libs)
{
    adx_context_t *context_info = new adx_context_t;
    if (!context_info) return NULL;

    // 初始化 redis
    if (redis_init(conf, context_info))
	return NULL;

    context_info->conf = conf;
    context_info->kafka_conn = kafka_conn;
    context_info->link_jump_conn = link_jump_conn;
    context_info->price_libs = price_libs;
    return context_info;
}

#if 10
int main(int argc, char *argv[])
{
    int errcode = E_SUCCESS;

    // init fcgi
    FCGX_Init();

    // init signal
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    // 初始化配置文件
    adx_dsp_conf_t *conf = adx_dsp_conf_load();
    if (!conf) return -1;

    // 初始化本地日志
    errcode = adx_log_init(conf->log_path, "tracker", conf->log_level);
    if (errcode != E_SUCCESS) return errcode;
#ifndef __DEBUG__
    // kafka conn
    adx_kafka_conn_t *kafka_conn = adx_kafka_conn(conf->kafka_broker, conf->kafka_topic);
    if (!kafka_conn) return -1;
#endif
    adx_kafka_conn_t *link_jump_conn = adx_kafka_conn(conf->link_jump_broker_list, conf->link_jump_topic);
    if (!link_jump_conn) return -1;

    // init flume thread
    // errcode = adx_flume_init(conf->flume_ip, conf->flume_port);
    // if (errcode != E_SUCCESS) return errcode;

    // redis数据dump到内存
    errcode = adx_dump_init(conf);
    if (errcode != E_SUCCESS) return errcode;
#ifdef __DEBUG__
    // adx_cache_display(get_adx_cache_root()); // 打印dump 结构
#endif
    // 初始化 解密价格动态库
    vector<adx_price_lib_path> price_libs;
    errcode = adx_price_lib_path_init(conf, price_libs);
    if (errcode != E_SUCCESS) return errcode;

    // 投放控制初始化
    errcode = adx_dsp_control_init(conf);
    if (errcode != E_SUCCESS) return errcode;

#ifndef __DEBUG__
    pthread_t tids[conf->cpu_count];
    for (int i = 0; i < conf->cpu_count; i++) {
	adx_context_t *context_info = context_info_init(conf, kafka_conn, link_jump_conn, price_libs); // init context_info
	if (!context_info) return -1;

	int errcode = pthread_create(&tids[i], NULL, adx_dsp_thread, context_info);
	if (errcode) {
	    cout << "pthread_create err : " << errcode << endl;
	    return E_CREATETHREAD;
	}
    }

    for (int i = 0; i < conf->cpu_count; i++) {
	pthread_join(tids[i], NULL);
    }
#else
    adx_context_t *context_info = context_info_init(conf, NULL, NULL, price_libs);
    if (!context_info) return -1;
    adx_dsp_thread(context_info);
#endif

    // TODO: free
    return 0;
}
#endif








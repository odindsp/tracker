
#ifndef DSP_TRACKER_H_
#define DSP_TRACKER_H_

#include <iostream>
#include <vector>

#include <dlfcn.h>
#include <fcgiapp.h>
#include <fcgi_config.h>
#include <hiredis/hircluster.h>

#include "errorcode.h"
#include "adx_cache.h"	
#include "adx_log.h"
#include "adx_alloc.h"
#include "adx_kafka.h"

using namespace std;

// 配置文件
typedef struct {

    int cpu_count;

    char *log_path; // 本地日志
    int log_level; // 日志等级

    char *AdSetting_ip;
    int AdSetting_port;
    char *AdSetting_pass;

    char *AdController_ip;
    int AdController_port;
    char *AdController_pass;

    char *AdCounter_ip;
    int AdCounter_port;
    char *AdCounter_pass;

    char *flume_ip;
    int flume_port;

    char *kafka_broker;
    char *kafka_topic;

    // int price_upper;
    // 临时处理用友数据
    char *link_jump_topic;
    char *link_jump_broker_list;

    char *activation_url; // 激活URL 更新redis时间保持激活状态
    char *activation_parameter; // 激活URL 参数。用于判定是否是激活URL

} adx_dsp_conf_t;

// 原始URL参数
typedef struct {
    string		remoteaddr;		// 客户端ip地址
    string		ip;			// url中包含的客户端ip地址
    char		mtype;			// 请求类别: 赢价w，展示i，点击c
    string		bid;			// 请求id
    int		at;			// 竞价类型: RTB=0 合同=1
    int		mapid;			// MAP对应 用于查询creativeid/../..
    string		impid;			// 广告展示机会id
    string		impt;			// 广告展示机会类型
    string		impm;			// 广告展示机会类型所对应的展示形式
    string		w;			// 广告展示机会宽度
    string		h;			// 广告展示机会高度
    string		deviceid;		// 设备id
    int		deviceidtype;		// 设备类型
    int		adx;			// 渠道id
    string		price;			// 价格解密前密文
    string		curl;			// 302跳转
    string		appid;			// 媒体id
    string		nw;			// 联网类型
    string		os;			// 操作系统
    string		gp;			// 地理位置
    string		tp;			// 移动设备类型
    string		mb;			// 移动设备品牌
    string		md;			// 移动设备型号
    string		op;			// 移动设备运营商
    string		ds;			// 数据平台来源: dsp/pap
    string		dealid;			// 合同id
    string		advid;			// 部分adx的请求数据中，一个展现位(imp)，包含多个子广告位，用advid来标识。其他不支持的adx，该参数值为0。

    string		activation_parameter;  // 激活URL 参数
    
    string		ua;

} adx_url_info_t;

// DSP业务逻辑中间数据结构
typedef struct {
    int iv;				// 有效/无效
    int ic;				// 追踪请求失效原因
    double price;			// 价格解密后
    FCGX_Request *request;		// FCGI具柄
    adx_pool_t *pool;

} adx_dsp_info_t;

// 解密价格动态库路径
typedef struct {
    int adx;
    void *dlopen_handle;
    char *path;

} adx_price_lib_path;

// 包含 配置文件/URL原始参数/业务逻辑中间数据/服务器连接具柄
typedef struct {

    adx_dsp_conf_t			*conf;			// 配置文件
    adx_url_info_t			url_info;		// URL 原始参数
    adx_dsp_info_t			dsp_info;		// 业务逻辑使用的中间数据

    redisContext			*AdSetting_slave;	// redis 物料
    redisContext			*AdController_master;	// redis 投放控制
    redisClusterContext		*AdCounter_master;	// redis集群 展现/点击/价格(累加)

    adx_kafka_conn_t		*kafka_conn;		// kafka conn
    adx_kafka_conn_t		*link_jump_conn;	// 用友历史数据
    vector<adx_price_lib_path>	price_libs;		// 解密价格动态库

} adx_context_t;

bool get_run_flag();
int adx_dsp_doit(FCGX_Request *request,  adx_pool_t *pool, adx_context_t *context_info);
bool check_frequency_validity(adx_context_t &adx_context);

adx_dsp_conf_t *adx_dsp_conf_load();
int adx_dump_init(adx_dsp_conf_t *conf);

#endif // DSP_TRACKER_H_





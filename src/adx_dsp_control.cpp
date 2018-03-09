
#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>

#include "adx_curl.h"
#include "adx_dump.h"
#include "adx_util.h"
#include "adx_time.h"
#include "adx_redis.h"
#include "adx_string.h"
#include "adx_dsp_control.h"
#include "tracker.h"

#define TIME_MONTH_DAY() get_time_str_r(dsp_info->pool, 6, NULL)
#define TIME_HOUR_10M() get_time_hour_split(10)
#define END_DATE_TIME() get_enddatetime(frequencyperiod, frequencyendtime)
#define END_DATE_TIME_SEC (END_DATE_TIME() - time(NULL))

static int liveflag = 0;
int get_liveflag(){return liveflag;}
void set_liveflag(int _liveflag){liveflag = _liveflag;}

// 保持激活线程
void *adx_dsp_control_activation_url(void *arg)
{
    char *activation_url = (char *)arg;
    pthread_detach(pthread_self());

    while(get_run_flag()) {

	int errcode = adx_curl_open(activation_url);
	if (errcode != E_SUCCESS) {
	    // TODO: 重发机制
	}

	sleep(60); 
    }

    pthread_exit(NULL);
}

// 更新投放控制redis中 dsp_adcontrol_working 当前服务器时间的时间戳
void adx_dsp_control_update_working(adx_dsp_control_context_t *context_info)
{
    int errcode = adx_redis_command(context_info->AdController_master, "SET dsp_adcontrol_working %d", time(NULL));
    if (errcode != E_SUCCESS) {
	sleep(10);
    }
}

//计数redis 10分钟时段清零 --清除当前时段向后间隔2个时段后的时段值 例:当前时段为0, 则清除时段值为3的内容
void adx_dsp_control_10m_clear(adx_dsp_control_context_t *context_info)
{
    int segment = (TIME_HOUR_10M() + 3) % 6;
    redisClusterContext *conn = context_info->AdCounter_master;

    adx_cache_t *policy_cache = adx_cache_find(get_adx_cache_root(), CACHE_STR("policy")); // find root ==> policy
    if (policy_cache) {

	adx_list_t *p = NULL;
	adx_list_for_each(p, &policy_cache->child_list) { // 遍历 policy
	    adx_cache_t *node = adx_list_entry(p, adx_cache_t, next_list);
	    adx_size_t policyid = adx_cache_value_to_number(node->key); // 遍历全部policyid
	    adx_size_t campaignid = GET_DUMP_VALUE_INT(node, "campaignid"); // 获取policyid 对应的 campaignid

	    adx_redis_cluster_command(conn, "HDEL dspcounter_%lu 10m_imp_p_%lu_%d", campaignid, policyid, segment);
	    adx_redis_cluster_command(conn, "HDEL dspcounter_%lu 10m_clk_p_%lu_%d", campaignid, policyid, segment);
	}
    }
}

// 更新dsp_adcontrol_working线程
void *adx_dsp_control_update(void *arg)
{
    pthread_detach(pthread_self());
    adx_dsp_control_context_t *context_info = (adx_dsp_control_context_t *)arg;

    while(get_run_flag()) {

	// fprintf(stdout, "liveflag=%d\n", liveflag);
	if (liveflag) {
	    // 更新投放控制redis中 dsp_adcontrol_working
	    adx_dsp_control_update_working(context_info);
	}

	// 计数redis 10分钟时段 清零
	// adx_dsp_control_10m_clear(context_info);

	liveflag = 0;
	sleep(60);
    }

    pthread_exit(NULL);
}

// 投放控制初始化
int adx_dsp_control_init(adx_dsp_conf_t *conf)
{
    int errcode = E_SUCCESS;

    /**********************************/
    /* 创建线程需要的连接句柄 */
    /**********************************/

    adx_dsp_control_context_t *context_info = (adx_dsp_control_context_t *)je_malloc(sizeof(adx_dsp_control_context_t));
    if (!context_info) return E_MALLOC;

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

    /**********************************/
    /* 创建线程 */
    /**********************************/
    // TODO: 线程分离
    pthread_t tid;
    errcode = pthread_create(&tid, NULL, adx_dsp_control_update, context_info);
    if (errcode != E_SUCCESS) return errcode;

    errcode = pthread_create(&tid, NULL, adx_dsp_control_activation_url, conf->activation_url);
    return errcode;
}

const char *get_deviceidtype(int type)
{
    switch (type) {
	case 0x00 : return "unknow_unknow";
	case 0x10 : return "imei_Clear";
	case 0x11 : return "imei_SHA1";
	case 0x12 : return "imei_md5";
	case 0x20 : return "mac_Clear";
	case 0x21 : return "mac_SHA1";
	case 0x22 : return "mac_md5";
	case 0x60 : return "androidid_Clear";
	case 0x61 : return "androidid_SHA1";
	case 0x62 : return "androidid_md5";
	case 0x70 : return "idfa_Clear";
	case 0x71 : return "idfa_SHA1";
	case 0x72 : return "idfa_md5";
	case 0xFF : return "other_other";
    }

    return "other_other";
}

time_t get_enddatetime(int frequencyperiod, int frequencyendtime)
{
    /* 
     * frequencyperiod = 1
     * 当前小时的最后59分59秒(10位时间戳)
     *
     * frequencyperiod = 2
     * 当天的23点59分59秒(10位时间戳)
     *
     * frequencyperiod = 3
     * 当周的最后一天的23点59分59秒(10位时间戳)
     *
     * frequencyperiod = 4
     * 当月最后一天的23点59分59秒(10位时间戳)
     *
     * frequencyperiod = 0
     * 全周期: frequencyendtime
     */

    switch(frequencyperiod) {
	case 1 : return get_time_hour_end();
	case 2 : return get_time_day_end();
	case 3 : return get_time_week_end();
	case 4 : return get_time_month_end();
    }

    return frequencyendtime;
}

// 计数流程
int adx_dsp_control_count(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count)
{
    /**********************************/
    /* URL 参数变量 */
    /**********************************/
    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    adx_size_t mapid = url_info->mapid;
    redisClusterContext *conn = context_info->AdCounter_master;

    /*********************************************/
    /* mapid 对应 campaignid/policyid/creativeid */
    /*********************************************/
    adx_size_t campaignid = dsp_count->campaignid = adx_dump_get_campaignid(mapid);
    adx_size_t policyid = dsp_count->policyid = adx_dump_get_policyid(mapid);
    adx_size_t creativeid = dsp_count->creativeid = adx_dump_get_creativeid(mapid);
    if (!campaignid || !policyid || !creativeid) {
	adx_log(LOGERROR, "[%c][%d][%s][0x%X][adx_dsp_control_count][mapid=%lu][campaignid=%lu][policyid=%lu][creativeid=%lu]",
		mtype, url_info->adx, bid, E_TRACK_UNDEFINE_MAPID, mapid, campaignid, policyid, creativeid);
	return E_TRACK_UNDEFINE_MAPID;
    }

    adx_log(LOGINFO, "[%c][%d][%s][0x0][adx_dsp_control_count][mapid=%lu][campaignid=%lu][policyid=%lu][creativeid=%lu][day=%s]",
	    mtype, url_info->adx,  bid, mapid, campaignid, policyid, creativeid, TIME_MONTH_DAY());

    /**********************************/
    /* 展现/点击/匀速投/累计成本 计数 */
    /**********************************/
    // key dspcounter_(campaignid)

    // field imp_c:活动展示数总计数
    // field clk_c:活动点击数总计数
    // field cost_c:活动累计花费总计

    // field imp_p_(policyid):投放策略展示总计数
    // field clk_p_(policyid):投放策略点击总计数
    // field cost_p_(policyid):投放策略累计花费总计数

    // field d_imp_c_(月日:0101-12-31):单日活动展示计数
    // field d_clk_c_(月日:0101-12-31):单日活动点击计数
    // field d_cost_c_(月日:0101-12-31):单日活动累计花费

    // field d_imp_p_(policyid)_(月日:0101-12-31):单日投放策略展示计数
    // field d_clk_p_(policyid)_(月日:0101-12-31):单日投放策略点击计数
    // field d_cost_p_(policyid)_(月日:0101-12-31):单日投放策略累计花费

    /* 10分钟计数 匀速投放/自动出价 */
    // key dsp10minutecounter_(policyid)_(10分钟段)
    // field imp_(at)_(mapid)_(adxcode)_(regioncode)	= 10分钟时段内展示计数
    // field clk_(at)_(mapid)_(adxcode)_(regioncode)	= 10分钟时段内点击计数
    // field cost_(at)_(mapid)_(adxcode)_(regioncode)	= 10分钟时段的投放成本


    char MONTH_DAY[128];
    get_time_str(6, "_", MONTH_DAY);

    // 展现计数
    if (mtype == 'i') {

	errcode = adx_redis_cluster_command(conn, dsp_count->c_imp,	"HINCRBY dspcounter_%lu imp_c 1", campaignid);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_imp,	"HINCRBY dspcounter_%lu imp_p_%lu 1", campaignid, policyid);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_imp_day,	"HINCRBY dspcounter_%lu d_imp_c_%s 1", campaignid, TIME_MONTH_DAY());
	errcode = adx_redis_cluster_command(conn, dsp_count->p_imp_day,	"HINCRBY dspcounter_%lu d_imp_p_%lu_%s 1", campaignid, policyid, TIME_MONTH_DAY());

	// 获取点击 计算 KPI/频次 等
	errcode = adx_redis_cluster_command(conn, dsp_count->c_clk,	"HGET dspcounter_%lu clk_c", campaignid);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_clk,	"HGET dspcounter_%lu clk_p_%lu", campaignid, policyid);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_clk_day,	"HGET dspcounter_%lu d_clk_c_%s", campaignid, TIME_MONTH_DAY());
	errcode = adx_redis_cluster_command(conn, dsp_count->p_clk_day,	"HGET dspcounter_%lu d_clk_p_%lu_%s", campaignid, policyid, TIME_MONTH_DAY());

	errcode = adx_redis_cluster_command(conn, dsp_count->p_imp_10m,	"HINCRBY dsp10minutecounter_%lu_%d imp_count 1", // 匀速
		policyid, TIME_HOUR_10M());
	errcode = adx_redis_cluster_command(conn, dsp_count->p_clk_10m,	"HGET dsp10minutecounter_%lu_%d clk_count", // 匀速
		policyid, TIME_HOUR_10M());

	errcode = adx_redis_cluster_command(conn, "HINCRBY  auto_cost_bid_%lu_%s imp_%d_%lu_%d_%s 1", // 自动出价
		policyid, MONTH_DAY,
		url_info->at, mapid, url_info->adx, url_info->gp.c_str());

	fprintf(stdout, "HINCRBY  auto_cost_bid_%lu_%s imp_%d_%lu_%d_%s 1", // 自动出价
		policyid, MONTH_DAY,
		url_info->at, mapid, url_info->adx, url_info->gp.c_str());
    }

    // 点击计数
    if (mtype == 'c') {

	// 获取展现 计算 KPI/频次 等
	errcode = adx_redis_cluster_command(conn, dsp_count->c_imp,	"HGET dspcounter_%lu imp_c", campaignid);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_imp,	"HGET dspcounter_%lu imp_p_%lu", campaignid, policyid);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_imp_day,	"HGET dspcounter_%lu d_imp_c_%s", campaignid, TIME_MONTH_DAY());
	errcode = adx_redis_cluster_command(conn, dsp_count->p_imp_day,	"HGET dspcounter_%lu d_imp_p_%lu_%s", campaignid, policyid, TIME_MONTH_DAY());

	errcode = adx_redis_cluster_command(conn, dsp_count->c_clk,	"HINCRBY dspcounter_%lu clk_c 1", campaignid);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_clk,	"HINCRBY dspcounter_%lu clk_p_%lu 1", campaignid, policyid);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_clk_day,	"HINCRBY dspcounter_%lu d_clk_c_%s 1", campaignid, TIME_MONTH_DAY());
	errcode = adx_redis_cluster_command(conn, dsp_count->p_clk_day,	"HINCRBY dspcounter_%lu d_clk_p_%lu_%s 1", campaignid, policyid, TIME_MONTH_DAY());

	errcode = adx_redis_cluster_command(conn, dsp_count->p_imp_10m,	"HGET dsp10minutecounter_%lu_%d imp_count", // 匀速
		policyid, TIME_HOUR_10M()); 
	errcode = adx_redis_cluster_command(conn, dsp_count->p_clk_10m,	"HINCRBY dsp10minutecounter_%lu_%d clk_count 1", // 匀速
		policyid, TIME_HOUR_10M());

	errcode = adx_redis_cluster_command(conn, "HINCRBY  auto_cost_bid_%lu_%s clk_%d_%lu_%d_%s 1", // 自动出价
		policyid, MONTH_DAY,
		url_info->at, mapid, url_info->adx, url_info->gp.c_str());
    }

    // 费用计数
    if (dsp_info->price) {

	adx_size_t price = adx_size_t(dsp_info->price * 100);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_cost,	"HINCRBY dspcounter_%lu cost_c %lu", campaignid, price);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_cost,	"HINCRBY dspcounter_%lu cost_p_%lu %lu", campaignid, policyid, price);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_cost_day,"HINCRBY dspcounter_%lu d_cost_c_%s %lu", campaignid, TIME_MONTH_DAY(), price);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_cost_day,"HINCRBY dspcounter_%lu d_cost_p_%lu_%s %lu", campaignid, policyid, TIME_MONTH_DAY(), price);

	errcode = adx_redis_cluster_command(conn, dsp_count->p_cost_10m,"HINCRBY dsp10minutecounter_%lu_%d cost_count %d", // 匀速
		policyid, TIME_HOUR_10M(), price);

	errcode = adx_redis_cluster_command(conn, "HINCRBY auto_cost_bid_%lu_%s cost_%d_%ld_%d_%s %d", // 自动出价计数
		policyid, MONTH_DAY,
		url_info->at, mapid, url_info->adx, url_info->gp.c_str(),// field
		price);// value
    } else {

	errcode = adx_redis_cluster_command(conn, dsp_count->c_cost,	"HGET dspcounter_%lu cost_c", campaignid);
	errcode = adx_redis_cluster_command(conn, dsp_count->p_cost,	"HGET dspcounter_%lu cost_p_%lu", campaignid, policyid);
	errcode = adx_redis_cluster_command(conn, dsp_count->c_cost_day,"HGET dspcounter_%lu d_cost_c_%s", campaignid, TIME_MONTH_DAY());
	errcode = adx_redis_cluster_command(conn, dsp_count->p_cost_day,"HGET dspcounter_%lu d_cost_p_%lu_%s", campaignid, policyid, TIME_MONTH_DAY());

	errcode = adx_redis_cluster_command(conn, dsp_count->p_cost_10m,"HGET dsp10minutecounter_%lu_%d cost_count", // 匀速
		policyid, TIME_HOUR_10M());
    }

    // 10分钟计数销毁时间
    errcode = adx_redis_cluster_command(conn, "EXPIRE dsp10minutecounter_%lu_%d %d", policyid, TIME_HOUR_10M(), 1800);
    if (errcode != E_SUCCESS) {
	// TODO: 错误处理
    }

    // 自动出价格计数销毁时间  key: auto_cost_bid_(policyid)_(月日)
    errcode = adx_redis_cluster_command(conn, "EXPIRE auto_cost_bid_%lu_%s %d",
	    policyid, MONTH_DAY,
	    get_time_day_end() - time(NULL));
    if (errcode != E_SUCCESS) {
	// TODO: 错误处理
    }

    /**********************************/
    /* 频次计数 */
    /**********************************/
    // key did_(deviceidtype)_(deviceid):人群包黑白名单及频次控制计数(HASH)
    // ap_(audienceid):当前设备id所属的人群包(value暂无用)
    // bl_global:当前设备id属于平台黑名单(value暂无用)
    // fc_(campaignid)_(action)_(period)_(enddatetime):活动频次控制计数
    // fc_(policyid)_(action)_(period)_(enddatetime):策略频次控制计数
    // fc_(creativeid)_(action)_(period)_(enddatetime):创意频次控制计数
    const char *deviceid = dsp_count->deviceid = url_info->deviceid.c_str();
    const char *deviceidtype = dsp_count->deviceidtype = get_deviceidtype(url_info->deviceidtype);
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][deviceidtype=%d][deviceidtype=%s][deviceid=%s]",
	    mtype, url_info->adx,  bid, errcode,
	    url_info->deviceidtype, deviceidtype, deviceid);

    /**********************************/
    /* 活动/创意 频次计数 */
    /**********************************/
    // dsp_campaign_control_(campaignid): 活动control
    // frequencytype:频次控制类型：0不限制 1活动频次 2创意的频次
    // frequencyaction:频次控制行为: 1展现 2点击
    // frequencyperiod:频次控制周期:  0全周期 1小时 2天 3周 4月
    // frequencyendtime:全周期频次控制结束日期: 时间戳
    adx_cache_t *campaign_control = dsp_count->campaign_control;
    int frequencytype = GET_DUMP_VALUE_INT(campaign_control, "frequencytype");
    int frequencyaction = GET_DUMP_VALUE_INT(campaign_control, "frequencyaction");
    int frequencyperiod = GET_DUMP_VALUE_INT(campaign_control, "frequencyperiod");
    int frequencyendtime = GET_DUMP_VALUE_INT(campaign_control, "frequencyendtime");
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][campaign_control][frequencytype=%d][frequencyaction=%d][frequencyperiod=%d][frequencyendtime=%d]",
	    mtype,  url_info->adx, bid, errcode,
	    frequencytype, frequencyaction, frequencyperiod, frequencyendtime);

    if ((mtype == 'i' && frequencyaction == 1) || (mtype == 'c' && frequencyaction == 2)) {

	if (frequencytype == 1) {
	    // fc_(campaignid)_(action)_(period)_(enddatetime)	
	    errcode = adx_redis_cluster_command(conn, dsp_count->fc_campaign, "HINCRBY did_%s_%s fc_%lu_%d_%d_%ld 1",
		    deviceidtype, deviceid, campaignid, frequencyaction, frequencyperiod, END_DATE_TIME());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][frequencytype=campaign][key=did_%s_%s][field=fc_%lu_%d_%d_%ld][res=%lu]",
		    mtype,  url_info->adx, bid, errcode,
		    deviceidtype, deviceid, campaignid, frequencyaction, frequencyperiod, END_DATE_TIME(),
		    dsp_count->fc_campaign);

	    //写清除索引到redis集群
	    errcode = adx_redis_cluster_command(conn, "SADD del_%ld did_%s_%s&$&fc_%lu_%d_%d_%ld",
		    END_DATE_TIME(), deviceidtype, deviceid, campaignid, frequencyaction, frequencyperiod, END_DATE_TIME());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][frequencytype=campaign][key=del_%ld][value=did_%s_%s&$&fc_%lu_%d_%d_%ld]",
		    mtype, url_info->adx,  bid, errcode,
		    END_DATE_TIME(),
		    deviceidtype, deviceid, campaignid, frequencyaction, frequencyperiod, END_DATE_TIME());
	}

	if (frequencytype == 2) {
	    // fc_(creativeid)_(action)_(period)_(enddatetime):创意频次控制计数
	    errcode = adx_redis_cluster_command(conn, dsp_count->fc_creative, "HINCRBY did_%s_%s fc_%lu_%d_%d_%ld 1",
		    deviceidtype, deviceid, creativeid, frequencyaction, frequencyperiod, END_DATE_TIME());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][frequencytype=creative][key=did_%s_%s][field=fc_%lu_%d_%d_%ld][res=%lu]",
		    mtype, url_info->adx,  bid, errcode,
		    deviceidtype, deviceid, creativeid, frequencyaction, frequencyperiod, END_DATE_TIME(), dsp_count->fc_creative);

	    //写清除索引到redis集群
	    errcode = adx_redis_cluster_command(conn, "SADD del_%ld did_%s_%s&$&fc_%lu_%d_%d_%ld",
		    END_DATE_TIME(), deviceidtype, deviceid, creativeid, frequencyaction, frequencyperiod, END_DATE_TIME());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][frequencytype=creative][key=del_%ld][value=did_%s_%s&$&fc_%lu_%d_%d_%ld]",
		    mtype, url_info->adx,  bid, errcode,
		    END_DATE_TIME(),
		    deviceidtype, deviceid, creativeid, frequencyaction, frequencyperiod, END_DATE_TIME());
	}
    }

    /**********************************/
    /* 策略 频次计数 */
    /**********************************/
    // dsp_policy_control_(policyid):策略control
    // frequencytype:频次控制类型：0不限制 1策略的频次
    // frequencyaction:频次控制行为: 1展现 2点击
    // frequencyperiod:频次控制周期:  0全周期 1小时 2天 3周 4月
    // frequencyendtime:全周期频次控制结束日期: 时间戳
    adx_cache_t *policy_control = dsp_count->policy_control;
    frequencytype = GET_DUMP_VALUE_INT(policy_control, "frequencytype");
    frequencyaction = GET_DUMP_VALUE_INT(policy_control, "frequencyaction");
    frequencyperiod = GET_DUMP_VALUE_INT(policy_control, "frequencyperiod");
    frequencyendtime = GET_DUMP_VALUE_INT(policy_control, "frequencyendtime");
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][policy_control][frequencytype=%d][frequencyaction=%d][frequencyperio=%d][frequencyendtime=%d]",
	    mtype,  url_info->adx, bid, errcode,
	    frequencytype, frequencyaction, frequencyperiod, frequencyendtime);

    if ((mtype == 'i' && frequencyaction == 1) || (mtype == 'c' && frequencyaction == 2)) {

	if (frequencytype == 1) {
	    // fc_(policyid)_(action)_(period)_(enddatetime):策略频次控制计数
	    errcode = adx_redis_cluster_command(conn, dsp_count->fc_policy, "HINCRBY did_%s_%s fc_%lu_%d_%d_%ld 1",
		    deviceidtype, deviceid, policyid, frequencyaction, frequencyperiod, END_DATE_TIME());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][frequencytype=policy][key=did_%s_%s][field=fc_%lu_%d_%d_%ld][res=%lu]",
		    mtype,  url_info->adx,  bid, errcode,
		    deviceidtype, deviceid, creativeid, frequencyaction, frequencyperiod, END_DATE_TIME(), dsp_count->fc_policy);

	    //写清除索引到redis集群
	    errcode = adx_redis_cluster_command(conn, "SADD del_%ld did_%s_%s&$&fc_%lu_%d_%d_%ld",
		    END_DATE_TIME(), deviceidtype, deviceid, policyid, frequencyaction, frequencyperiod, END_DATE_TIME());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_count][frequencytype=policy][key=del_%ld][value=did_%s_%s&$&fc_%lu_%d_%d_%ld]",
		    mtype,  url_info->adx, bid, errcode,
		    END_DATE_TIME(),
		    deviceidtype, deviceid, policyid, frequencyaction, frequencyperiod, END_DATE_TIME());
	}
    }

    return errcode;
}

int adx_dsp_control_kpi_campaign(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count) 
{
    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    redisContext *conn = context_info->AdController_master;
    adx_cache_t *campaign_control = dsp_count->campaign_control;

    /*********************************************/
    /* 活动KPI控制 */
    /*********************************************/
    // key dsp_campaign_control_(campaignid):活动的投放控制策略(HASH)
    // field totalimp:总展示数，-1或没有表示不限
    // field totalclk:总点击数，-1或没有表示不限
    // field totalbudget:总预算。-1表示不限，单位rmb 分/cpm*100
    // field dayimp_(月日):日展示量目标: -1不限
    // field dayclk_(月日):日点击量目标: -1不限
    // field daybudget_(月日):日预算: -1不限
    // int totalimp = GET_DUMP_VALUE_INT(campaign_control, "totalimp");
    // int totalclk = GET_DUMP_VALUE_INT(campaign_control, "totalclk");
    // int totalbudget = GET_DUMP_VALUE_INT(campaign_control, "totalbudget");
    adx_size_t dayimp_day = adx_cache_to_number(adx_cache_find_str(campaign_control, "dayimp_%s", TIME_MONTH_DAY()));
    adx_size_t dayclk_day = adx_cache_to_number(adx_cache_find_str(campaign_control, "dayclk_%s", TIME_MONTH_DAY()));
    adx_size_t daybudget_day = adx_cache_to_number(adx_cache_find_str(campaign_control, "daybudget_%s", TIME_MONTH_DAY()));
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][dayimp_day=%lu][dayclk_day=%lu][daybudget_day=%lu]",
	    mtype,  url_info->adx, bid, errcode,
	    dayimp_day, dayclk_day, daybudget_day);

    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][count_dayimp_day=%lu][count_dayclk_day=%lu][count_cost_day=%lu]",
	    mtype, url_info->adx,  bid, errcode,
	    dsp_count->c_imp_day, dsp_count->c_clk_day, dsp_count->c_cost_day);

    // 活动总展现
    // if (totalimp != -1 && dsp_count->c_imp >= totalimp) return adx_redis_command(conn, "setex pause_campaign_%d %d 11", dsp_count->campaignid, get_time_sec_today_end());

    // 日KPI
    dayimp_day = dayimp_day ? dayimp_day : -1;
    dayclk_day = dayclk_day ? dayclk_day : -1;
    if (dayimp_day == -1 && dayclk_day == -1) { // 不限
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][NOT KPI]", mtype, url_info->adx,  bid, 0);
	return E_SUCCESS;

    } else if (dayimp_day == -1) { // 限制点击
	if (dsp_count->c_clk_day >= dayclk_day) {
	    errcode = adx_redis_command(conn, "setex pause_campaign_%lu %d 11", dsp_count->campaignid, get_time_sec_today_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][PAUSE][SETEX][pause_campaign_%lu][%d][11]",
		    mtype, url_info->adx,  bid, errcode,
		    dsp_count->campaignid, get_time_sec_today_end());
	    return errcode;
	}

    } else if (dayclk_day == -1) { // 限制展现
	if (dsp_count->c_imp_day >= dayimp_day) {
	    errcode = adx_redis_command(conn, "setex pause_campaign_%lu %d 11", dsp_count->campaignid, get_time_sec_today_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][PAUSE][SETEX][pause_campaign_%lu][%d][11]",
		    mtype, url_info->adx,  bid, errcode,
		    dsp_count->campaignid, get_time_sec_today_end());
	    return errcode;
	}

    } else { // 同时限制

	if (dsp_count->c_imp_day >= dayimp_day && dsp_count->c_clk_day >= dayclk_day) {
	    errcode = adx_redis_command(conn, "setex pause_campaign_%lu %d 11", dsp_count->campaignid, get_time_sec_today_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][PAUSE][SETEX][pause_campaign_%lu][%d][11]",
		    mtype, url_info->adx,  bid, errcode,
		    dsp_count->campaignid, get_time_sec_today_end());
	    return errcode;
	}
    }

    // 活动总预算
    // if (totalbudget != -1 && dsp_count->c_cost >= totalbudget * 100) return adx_redis_command(conn, "setex pause_campaign_%d %d 12", dsp_count->campaignid, get_time_sec_today_end());

    // KPI 活动日预算
    daybudget_day = daybudget_day ? daybudget_day : -1;
    if (daybudget_day != -1 && dsp_count->c_cost_day / 1000 >= daybudget_day * 100) {
	errcode = adx_redis_command(conn, "setex pause_campaign_%lu %d 12", dsp_count->campaignid, get_time_sec_today_end());
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][PAUSE][SETEX][pause_campaign_%lu][%d][12]",
		mtype, url_info->adx,  bid, errcode,
		dsp_count->campaignid, get_time_sec_today_end());
	return errcode;
    }

    return errcode;
}

int adx_dsp_control_frequency_campaign(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count)
{
    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    redisContext *conn = context_info->AdController_master;
    adx_cache_t *campaign_control = dsp_count->campaign_control;

    /*********************************************/
    /* 活动 频次控制 */
    /*********************************************/
    // key dsp_campaign_control_(campaignid):活动的投放控制策略(HASH)
    // field frequencytype:频次控制类型：0不限制 1活动频次 2创意频次
    // field frequencyaction:频次控制行为: 1展现 2点击
    // field frequencycount:频次控制数量: frequencytype=0 无此字段
    int frequencytype = GET_DUMP_VALUE_INT(campaign_control, "frequencytype");
    int frequencyaction = GET_DUMP_VALUE_INT(campaign_control, "frequencyaction");
    adx_size_t frequencycount = GET_DUMP_VALUE_INT(campaign_control, "frequencycount");
    int frequencyperiod = GET_DUMP_VALUE_INT(campaign_control, "frequencyperiod");
    int frequencyendtime = GET_DUMP_VALUE_INT(campaign_control, "frequencyendtime");
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_campaign][frequencytype=%d][frequencyaction=%d][frequencycount=%lu]",
	    mtype, url_info->adx,  bid, errcode,
	    frequencytype, frequencyaction, frequencycount);

    if (frequencytype == 1) { // frequencytype 是否是活动
	if ((mtype == 'i' && frequencyaction == 1) || (mtype == 'c' && frequencyaction == 2)) { // frequencyaction 是否有效
	    if (dsp_count->fc_campaign >= frequencycount) { // 比较频次
		int errcode = adx_redis_command(conn, "SADD fc_campaign_%lu did_%s_%s", dsp_count->campaignid, dsp_count->deviceidtype, dsp_count->deviceid);
		adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_campaign][SADD][fc_campaign_%lu][did_%s_%s]",
			mtype, url_info->adx,  bid, errcode,
			dsp_count->campaignid, dsp_count->deviceidtype, dsp_count->deviceid);

		if (errcode != E_SUCCESS) return errcode;
		errcode = adx_redis_command(conn, "EXPIRE fc_campaign_%lu %d", dsp_count->campaignid, END_DATE_TIME_SEC);
		adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_campaign][EXPIRE][fc_campaign_%lu][%d]",
			mtype, url_info->adx,  bid, errcode,
			dsp_count->campaignid, END_DATE_TIME_SEC);
	    }
	}
    }

    return errcode;
}

void adx_auto_cost_send_queue(redisContext *conn, int policyid)
{
    int complete_time = 600 - get_time_sec_10min_end();
    adx_redis_command(conn, "HSET auto_cost_policy_complete %d %d", policyid, complete_time);

    // errcode = adx_redis_command(conn, "SREM auto_bid_policy_queue %d", policyid);
    // adx_redis_command(conn, "SADD auto_cost_policy_complete %d", policyid);
    // adx_redis_command(conn, "PUBLISH auto_cost_queue %s", mess.c_str());
}

int adx_dsp_control_kpi_policy(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count)
{
    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    redisContext *conn = context_info->AdController_master;
    adx_cache_t *policy_control = dsp_count->policy_control;

    /**********************************/
    /* 策略 KPI控制 */
    /**********************************/
    // dsp_policy_control_(policyid):策略的投放控制策略（HASH）
    // uniform:匀速投放: 0不匀速投放 1匀速投放
    // totalimp:总展示数，-1或没有表示不限
    // totalclk:总点击数，-1或没有表示不限
    // totalbudget:总预算。-1表示不限，单位rmb 分/cpm*100
    // allowtime_(月日):允许的时段: 24位二进制数
    // field dayimp_(月日):日展示量目标: -1不限
    // field dayclk_(月日):日点击量目标: -1不限		 
    // daybudget_(月日):日预算: -1不限
    // int totalimp = GET_DUMP_VALUE_INT(policy_control, "totalimp");
    // int totalclk = GET_DUMP_VALUE_INT(policy_control, "totalclk");
    // int totalbudget = GET_DUMP_VALUE_INT(policy_control, "totalbudget");
    adx_size_t dayimp_day = adx_cache_to_number(adx_cache_find_str(policy_control, "dayimp_%s", TIME_MONTH_DAY()));
    adx_size_t dayclk_day = adx_cache_to_number(adx_cache_find_str(policy_control, "dayclk_%s", TIME_MONTH_DAY()));
    adx_size_t daybudget_day = adx_cache_to_number(adx_cache_find_str(policy_control, "daybudget_%s", TIME_MONTH_DAY()));
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][dayimp_day=%lu][dayclk_day=%lu][daybudget_day=%lu]",
	    mtype, url_info->adx,  bid, errcode,
	    dayimp_day, dayclk_day, daybudget_day);

    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][count_dayimp_day=%lu][count_dayclk_day=%lu][count_cost_day=%lu]",
	    mtype, url_info->adx, bid, errcode,
	    dsp_count->p_imp_day, dsp_count->p_clk_day, dsp_count->p_cost_day);

    // 日KPI
    dayimp_day = dayimp_day ? dayimp_day : -1;
    dayclk_day = dayclk_day ? dayclk_day : -1;
    if (dayimp_day == -1 && dayclk_day == -1) { // 不限
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_campaign][NOT KPI]", mtype, url_info->adx, bid, 0);
	return E_SUCCESS;

    } else if (dayimp_day == -1) { // 限制点击
	if (dsp_count->p_clk_day >= dayclk_day) {
	    errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 21", dsp_count->policyid, get_time_sec_today_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][setex][pause_policy_%lu][%d][21]",
		    mtype,  url_info->adx, bid, errcode,
		    dsp_count->policyid, get_time_sec_today_end());
	   
	    adx_redis_command(conn, "SREM dsp_policyids %lu", dsp_count->policyid);
	    return errcode;
	}

    } else if (dayclk_day == -1) { // 限制展现
	if (dsp_count->p_imp_day >= dayimp_day) {
	    errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 21", dsp_count->policyid, get_time_sec_today_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][setex][pause_policy_%lu][%d][21]",
		    mtype, url_info->adx,  bid, errcode,
		    dsp_count->policyid, get_time_sec_today_end());
	    
	    adx_redis_command(conn, "SREM dsp_policyids %lu", dsp_count->policyid);
	    return errcode;
	}

    } else { // 同时限制
	if (dsp_count->p_imp_day >= dayimp_day && dsp_count->p_clk_day >= dayclk_day) {
	    errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 21", dsp_count->policyid, get_time_sec_today_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][setex][pause_policy_%lu][%d][21]",
		    mtype, url_info->adx, bid, errcode,
		    dsp_count->policyid, get_time_sec_today_end());
	    
	    adx_redis_command(conn, "SREM dsp_policyids %lu", dsp_count->policyid);
	    return errcode;
	}
    }

    // 策略日预算
    daybudget_day = daybudget_day ? daybudget_day : -1;
    if (daybudget_day != -1 && dsp_count->p_cost_day / 1000 >= daybudget_day * 100) {
	errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 22", dsp_count->policyid, get_time_sec_today_end());
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][setex][pause_policy_%lu][%d][22]",
		mtype, url_info->adx,  bid, errcode,
		dsp_count->policyid, get_time_sec_today_end());
	    
	adx_redis_command(conn, "SREM dsp_policyids %lu", dsp_count->policyid);
	return errcode;
    }

    /**********************************/
    /* 策略 匀速投放/允许时段 控制 */
    /**********************************/
    // key dsp_policy_control_(policyid):策略的投放控制策略（HASH）
    // field uniform:匀速投放: 0不匀速投放 1匀速投放
    // field allowtime_(月日) : 允许的时段: 24位二进制数
    // field dayimp_(月日):日展示量目标: -1不限
    // field dayclk_(月日):日点击量目标: -1不限		 
    int uniform = GET_DUMP_VALUE_INT(policy_control, "uniform");
    int autobid = GET_DUMP_VALUE_INT(policy_control, "autobid");
    int allowtime_day = adx_cache_to_number(adx_cache_find_str(policy_control, "allowtime_%s", TIME_MONTH_DAY()));
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][uniform=%d][allowtime_day=%d][%s]",
	    mtype, url_info->adx, bid, E_SUCCESS,
	    uniform, allowtime_day, int_to_binary(dsp_info->pool, allowtime_day, 24));

    // 是否是允许时段
    int allowtime_type = 0;
    if (allowtime_day & (1 << get_time_hour())) allowtime_type = 1;
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][allowtime=%s]",
	    mtype,  url_info->adx, bid, E_SUCCESS,
	    allowtime_type ? "YES" : "NO");
    // 不允许时段
    if (!allowtime_type) {
	errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 24", dsp_count->policyid, get_time_sec_hour_end());
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][setex][pause_policy_%lu][%d][24]",
		mtype,  url_info->adx, bid, errcode,
		dsp_count->policyid, get_time_sec_today_end());
	return errcode;
    }

    // 不是匀速投放
    // 自动出价 自动选择匀速投放
    if (uniform != 1 && autobid != 1) return E_SUCCESS;

    // 计算今日剩余 展现/点击 数量
    adx_size_t surplus_imp = dayimp_day - dsp_count->p_imp_day;
    adx_size_t surplus_clk = dayclk_day - dsp_count->p_clk_day;

    // 计算今日剩余 小时 数量
    adx_size_t surplus_hour = 0;
    for (int i = get_time_hour() + 1; i < 24; i++)
	if (allowtime_day & (1 << i)) surplus_hour++;

    // 计算今日剩余 10分钟 数量
    adx_size_t surplus_10m = (surplus_hour * 6) + (6 - get_time_hour_split(10));

    // 留20分钟的缓冲 如果不能提前20分钟完成目标 则后两个10分钟时段全速投放
    if (surplus_10m > 2) surplus_10m = surplus_10m - 2;
    else surplus_10m = 1;

    // 计算当前10分钟 剩余 展现/点击 数量
    // adx_size_t surplus_10m_imp = surplus_imp / surplus_10m;
    // adx_size_t surplus_10m_clk = surplus_clk / surplus_10m;
    adx_size_t surplus_10m_imp = adx_except_ceil(surplus_imp, surplus_10m);
    adx_size_t surplus_10m_clk = adx_except_ceil(surplus_clk, surplus_10m);

    // 剩余数量不足1个10分钟时段 (例如: 5 / 10 = 0)
    // if (surplus_10m_imp == 0) surplus_10m_imp = surplus_imp;
    // if (surplus_10m_clk == 0) surplus_10m_clk = surplus_clk;
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][surplus_10m=%ld][surplus_imp=%ld][surplus_10m_imp=%ld][count_imp=%ld][surplus_clk=%ld][surplus_10m_clk=%ld][count_cik=%ld]",
	    mtype,  url_info->adx, bid, errcode,
	    surplus_10m, 
	    surplus_imp, surplus_10m_imp, dsp_count->p_imp_10m, 
	    surplus_clk, surplus_10m_clk, dsp_count->p_clk_10m);

    // 判断当前10分钟 是否超过 数量
    if (dayimp_day == -1 && dayclk_day == -1) { // 不限
	return E_SUCCESS;

    } else if (dayimp_day == -1) { // 限点击
	if (dsp_count->p_clk_10m >= surplus_10m_clk) {
	    errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 23", dsp_count->policyid, get_time_sec_10min_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][uniform=YES][setex][pause_policy_%lu][%d][23]",
		    mtype, url_info->adx,  bid, errcode,
		    dsp_count->policyid, get_time_sec_10min_end());

	    if (autobid == 1) adx_auto_cost_send_queue(context_info->AdSetting_slave, dsp_count->policyid); // 自动出价
	}

    } else if (dayclk_day == -1) { // 限展现
	if (dsp_count->p_imp_10m >= surplus_10m_imp) {
	    errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 23", dsp_count->policyid, get_time_sec_10min_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][uniform=YES][setex][pause_policy_%lu][%d][23]",
		    mtype,  url_info->adx, bid, errcode,
		    dsp_count->policyid, get_time_sec_10min_end());

	    if (autobid == 1) adx_auto_cost_send_queue(context_info->AdSetting_slave, dsp_count->policyid); // 自动出价
	}

    } else { // 同时限制

	if (dsp_count->p_imp_10m >= surplus_10m_imp && dsp_count->p_clk_10m >= surplus_10m_clk) {
	    errcode = adx_redis_command(conn, "setex pause_policy_%lu %d 23", dsp_count->policyid, get_time_sec_10min_end());
	    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_kpi_policy][uniform=YES][setex][pause_policy_%lu][%d][23]",
		    mtype,  url_info->adx, bid, errcode,
		    dsp_count->policyid, get_time_sec_10min_end());

	    if (autobid == 1) adx_auto_cost_send_queue(context_info->AdSetting_slave, dsp_count->policyid); // 自动出价
	}
    }

    return errcode;
}

int adx_dsp_control_frequency_policy(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count)
{
    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    redisContext *conn = context_info->AdController_master;
    adx_cache_t *policy_control = dsp_count->policy_control;

    /**********************************/
    /* 策略频次控制 */
    /**********************************/
    // frequencytype:频次控制类型：0不限制 1策略频次
    // frequencyaction:频次控制行为: 1展现 2点击
    // frequencycount:频次控制数量: frequencytype=0 无此字段
    int frequencytype = GET_DUMP_VALUE_INT(policy_control, "frequencytype");
    int frequencyaction = GET_DUMP_VALUE_INT(policy_control, "frequencyaction");
    adx_size_t frequencycount = GET_DUMP_VALUE_INT(policy_control, "frequencycount");
    int frequencyperiod = GET_DUMP_VALUE_INT(policy_control, "frequencyperiod");
    int frequencyendtime = GET_DUMP_VALUE_INT(policy_control, "frequencyendtime");
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_policy][frequencytype=%d][frequencyaction=%d][frequencycount=%lu]",
	    mtype, url_info->adx,  bid, errcode,
	    frequencytype, frequencyaction, frequencycount);
    if (frequencytype == 1) { // frequencytype 是否是策略
	if ((mtype == 'i' && frequencyaction == 1) || (mtype == 'c' && frequencyaction == 2)) { // frequencyaction 是否有效
	    if (dsp_count->fc_policy >= frequencycount) { // 比较频次
		int errcode = adx_redis_command(conn, "SADD fc_policy_%lu did_%s_%s", dsp_count->policyid, dsp_count->deviceidtype, dsp_count->deviceid);
		adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_policy][SADD][fc_policy_%lu][did_%s_%s]",
			mtype,  url_info->adx, bid, errcode,
			dsp_count->policyid, dsp_count->deviceidtype, dsp_count->deviceid);

		if (errcode != E_SUCCESS) return errcode;
		errcode = adx_redis_command(conn, "EXPIRE fc_policy_%lu %d", dsp_count->policyid, END_DATE_TIME_SEC);
		adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_policy][EXPIRE][fc_policy_%lu][%d]",
			mtype, url_info->adx,  bid, errcode,
			dsp_count->policyid, END_DATE_TIME_SEC);
		return errcode;
	    }
	}
    }

    return errcode;
}

int adx_dsp_control_frequency_creative(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count)
{
    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    redisContext *conn = context_info->AdController_master;
    adx_cache_t *campaign_control = dsp_count->campaign_control;

    /*********************************************/
    /* 创意 频次控制 */
    /*********************************************/
    // key dsp_campaign_control_(campaignid):活动的投放控制策略(HASH)
    // field frequencytype:频次控制类型：0不限制 1活动频次 2创意频次
    // field frequencyaction:频次控制行为: 1展现 2点击
    // field frequencycount:频次控制数量: frequencytype=0 无此字段
    int frequencytype = GET_DUMP_VALUE_INT(campaign_control, "frequencytype");
    int frequencyaction = GET_DUMP_VALUE_INT(campaign_control, "frequencyaction");
    adx_size_t frequencycount = GET_DUMP_VALUE_INT(campaign_control, "frequencycount");
    int frequencyperiod = GET_DUMP_VALUE_INT(campaign_control, "frequencyperiod");
    int frequencyendtime = GET_DUMP_VALUE_INT(campaign_control, "frequencyendtime");
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_creative][frequencytype=%d][frequencyaction=%d][frequencycount=%lu]",
	    mtype, url_info->adx,  bid, errcode,
	    frequencytype, frequencyaction, frequencycount);

    if (frequencytype == 2) { // frequencytype 是否是创意
	if ((mtype == 'i' && frequencyaction == 1) || (mtype == 'c' && frequencyaction == 2)) { // frequencyaction 是否有效
	    if (dsp_count->fc_creative >= frequencycount) { // 对比频次
		errcode = adx_redis_command(conn, "SADD fc_creative_%lu did_%s_%s", dsp_count->creativeid, dsp_count->deviceidtype, dsp_count->deviceid);
		adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_creative][SADD][fc_creative_%lu][did_%s_%s]",
			mtype,  url_info->adx, bid, errcode,
			dsp_count->creativeid, dsp_count->deviceidtype, dsp_count->deviceid);

		if (errcode != E_SUCCESS) return errcode;
		errcode = adx_redis_command(conn, "EXPIRE fc_creative_%lu %d", dsp_count->creativeid, END_DATE_TIME_SEC);
		adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_frequency_creative][EXPIRE][fc_creative_%lu][%d]",
			mtype, url_info->adx,  bid, errcode,
			dsp_count->creativeid, END_DATE_TIME_SEC);
		return errcode;
	    }
	}
    }

    return errcode;
}

int adx_dsp_control_auto_cost_r(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, ADX_DSP_COUNTER_COUNT_T *dsp_count)
{

    int errcode = E_SUCCESS;
    char mtype = url_info->mtype;
    const char *bid = url_info->bid.c_str();
    adx_cache_t *policy_control = dsp_count->policy_control;

    /**********************************/
    /* 自动出价 计算10分钟10段 KPI */
    /**********************************/
    adx_size_t dayimp_day = adx_cache_to_number(adx_cache_find_str(policy_control, "dayimp_%s", TIME_MONTH_DAY()));
    adx_size_t dayclk_day = adx_cache_to_number(adx_cache_find_str(policy_control, "dayclk_%s", TIME_MONTH_DAY()));
    int allowtime_day = adx_cache_to_number(adx_cache_find_str(policy_control, "allowtime_%s", TIME_MONTH_DAY()));

    // 日KPI
    dayimp_day = dayimp_day ? dayimp_day : -1;
    dayclk_day = dayclk_day ? dayclk_day : -1;

    // 计算今日剩余 展现/点击 数量
    adx_size_t surplus_imp = dayimp_day - dsp_count->p_imp_day;
    adx_size_t surplus_clk = dayclk_day - dsp_count->p_clk_day;

    // 计算今日剩余 小时 数量
    adx_size_t surplus_hour = 0;
    for (int i = get_time_hour() + 1; i < 24; i++)
	if (allowtime_day & (1 << i)) surplus_hour++;

    // 计算今日剩余 10分钟 数量
    adx_size_t surplus_10m = (surplus_hour * 6) + (6 - get_time_hour_split(10));

    // 剩余10分钟时段 留20分钟的缓冲
    if (surplus_10m > 2) surplus_10m = surplus_10m - 2;
    else surplus_10m = 1;

    // 计算当前10分钟 剩余 展现/点击 数量
    adx_size_t surplus_10min_imp = surplus_imp / surplus_10m;
    adx_size_t surplus_10min_clk = surplus_clk / surplus_10m;

    time_t t = time(NULL);
    int policyid = dsp_count->policyid;
    int surplus_10min = surplus_10m;

    int daily_imp_kpi = dayimp_day;
    int daily_clk_kpi = dayclk_day;

    int daily_imp_count = dsp_count->p_imp_day;
    int daily_clk_count = dsp_count->p_clk_day;

    int this_10min_imp_kpi = surplus_10min_imp;
    int this_10min_clk_kpi = surplus_10min_clk;
    int next_10min_imp_kpi = surplus_10min_imp;
    int next_10min_clk_kpi = surplus_10min_clk;

    // 剩余数量不足1个10分钟时段 (例如: 5 / 10 = 0)
    if (surplus_10min_imp == 0) this_10min_imp_kpi = surplus_imp;
    if (surplus_10min_clk == 0) this_10min_clk_kpi = surplus_clk;

    int this_10min_imp_count = dsp_count->p_imp_10m;
    int this_10min_clk_count = dsp_count->p_clk_10m;

    string mess = "time=" + longToString(t);
    mess += "|policyid=" + intToString(policyid);
    mess += "|surplus_10min=" + intToString(surplus_10min);
    mess += "|count_key=dsp10minutecounter_" + intToString(policyid) + "_" + intToString(get_time_hour_split(10));
    mess += "|daily_imp_kpi=" + intToString(daily_imp_kpi);
    mess += "|daily_clk_kpi=" + intToString(daily_clk_kpi);
    mess += "|daily_imp_count=" + intToString(daily_imp_count);
    mess += "|daily_clk_count=" + intToString(daily_clk_count);
    mess += "|this_10min_imp_kpi=" + intToString(this_10min_imp_kpi);
    mess += "|this_10min_clk_kpi=" + intToString(this_10min_clk_kpi);
    mess += "|this_10min_imp_count=" + longToString(this_10min_imp_count);
    mess += "|this_10min_clk_count=" + longToString(this_10min_clk_count);
    mess += "|next_10min_imp_kpi=" + intToString(next_10min_imp_kpi);
    mess += "|next_10min_clk_kpi=" + intToString(next_10min_clk_kpi);
    mess += "|app=tracker";

    int kpi_10min_status = 0;
    if (dayimp_day == -1 && dayclk_day == -1) { // 不限

    } else if (dayimp_day == -1) { // 限点击
	if (dsp_count->p_clk_10m >= surplus_10min_clk)
	    kpi_10min_status = 1;

    } else if (dayclk_day == -1) { // 限展现
	if (dsp_count->p_imp_10m >= surplus_10min_imp)
	    kpi_10min_status = 1;

    } else { // 同时限制
	if (dsp_count->p_imp_10m >= surplus_10min_imp && dsp_count->p_clk_10m >= surplus_10min_clk) 
	    kpi_10min_status = 1;
    }

    if (kpi_10min_status) {
	errcode = adx_redis_command(context_info->AdSetting_slave, "SREM auto_bid_policy_queue %d", policyid);
	errcode = adx_redis_command(context_info->AdSetting_slave, "PUBLISH auto_cost_queue %s", mess.c_str());
    }

    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_dsp_control_auto_cost][%d][%s]",
	    mtype, url_info->adx,  bid, errcode,
	    kpi_10min_status, mess.c_str());

    return errcode;
}

int adx_dsp_control(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    /************************************/
    /* 计数逻辑与投放控制数据结构初始化 */
    /************************************/
    ADX_DSP_COUNTER_COUNT_T dsp_count;
    memset(&dsp_count, 0, sizeof(ADX_DSP_COUNTER_COUNT_T));

    dsp_count.campaign_control = adx_dump_get_campaign_control(url_info->mapid);
    dsp_count.policy_control = adx_dump_get_policy_control(url_info->mapid);
    if (!dsp_count.campaign_control || !dsp_count.policy_control) {
	adx_log(LOGERROR, "[%c][%d][%s][0x%X][adx_dsp_control][mapid=%lu][campaign_control=%p][policy_control=%p]",
		url_info->mtype, url_info->adx,  url_info->bid.c_str(), E_REDIS_GROUP_FC_INVALID,
		url_info->mapid, dsp_count.campaign_control, dsp_count.policy_control);
	return E_REDIS_GROUP_FC_INVALID;
    }

    /**********************************/
    /* 计数流程 */
    /**********************************/
    int errcode = adx_dsp_control_count(context_info, url_info, dsp_info, &dsp_count);

    /**********************************/
    /* 投放控制流程 */
    /**********************************/
    errcode = adx_dsp_control_kpi_campaign(context_info, url_info, dsp_info, &dsp_count);
    errcode = adx_dsp_control_kpi_policy(context_info, url_info, dsp_info, &dsp_count);
    errcode = adx_dsp_control_frequency_campaign(context_info, url_info, dsp_info, &dsp_count);
    errcode = adx_dsp_control_frequency_policy(context_info, url_info, dsp_info, &dsp_count);
    errcode = adx_dsp_control_frequency_creative(context_info, url_info, dsp_info, &dsp_count);

    return errcode;
}





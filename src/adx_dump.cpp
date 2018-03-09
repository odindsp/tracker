
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sstream>

#include "errorcode.h"
#include "tracker.h"

#include "adx_time.h"
#include "adx_dump.h"
#include "adx_json.h"
#include "adx_time.h"
#include "adx_util.h"
#include "adx_conf_file.h"
#include "adx_log.h"

static adx_cache_t *cache_root = NULL;
adx_cache_t *get_adx_cache_root(){return cache_root;}

typedef struct {
    redisContext *adx_setting; // redis 物料
    redisContext *adx_controller; // redis 投放控制
    redisClusterContext *adx_counter; // redis集群 展现/点击/价格/匀速/KPI/频次等 

} adx_dump_redis_conn;

int adx_dump_check_redis(redisContext *conn)
{

    adx_size_t value = 0;
    int errcode = adx_redis_command(conn, value, "GET dsp_adsetting_working");
    if (errcode != E_SUCCESS) return errcode;

    if (time(NULL) - value > (60 * 15))
	return E_REDIS_DUMP_TIME;

    return errcode;
}

int adx_dump_load_campaign_control(redisContext *conn, adx_cache_t *root)
{
    int errcode = E_SUCCESS;

    vector<adx_size_t> campaignids;
    errcode = adx_redis_command(conn, campaignids, "SMEMBERS dsp_campaignids"); // 全部 campaignid
    if (errcode != E_SUCCESS) return errcode;

    adx_cache_t *campaign_cache = adx_cache_add(root, CACHE_STR("campaign"), CACHE_NULL());  // 创建  root ==> campaign
    if (!campaign_cache) return E_MALLOC;

    // 遍历campaignids
    for (size_t i = 0; i < campaignids.size(); i++) {
	int campaign_id = campaignids[i];

	map<string, adx_size_t> campaign_control_info;
	errcode = adx_redis_command(conn, campaign_control_info, "HGETALL dsp_campaign_control_%d", campaign_id); // redis campaign control 信息
	if (errcode == E_SUCCESS) {

	    // 创建 root ==> campaign ==> campaign_id
	    adx_cache_t *campaign_child_campaignid = adx_cache_add(campaign_cache, CACHE_NUM(campaign_id), CACHE_NULL());

	    // 添加到campaign_id
	    map <string, adx_size_t>::iterator iter;
	    for(iter = campaign_control_info.begin(); iter != campaign_control_info.end(); iter++) {
		adx_cache_add(campaign_child_campaignid,
			CACHE_STR(iter->first.c_str()),
			CACHE_NUM(iter->second));

		// fprintf(stdout, "[%d][%s][%lu]\n", campaign_id, iter->first.c_str(), iter->second);
	    }
	}
    }

    // adx_cache_display(campaign_cache);
    return E_SUCCESS;
}

int adx_dump_load_policy_control(redisContext *conn, adx_cache_t *root)
{
    int errcode = E_SUCCESS;

    vector<adx_size_t> policyids;
    errcode = adx_redis_command(conn, policyids, "SMEMBERS dsp_policyids"); // 全部 policyid
    if (errcode != E_SUCCESS) return errcode;

    adx_cache_t *policy_cache = adx_cache_add(root, CACHE_STR("policy"), CACHE_NULL()); // 创建 root ==> policy
    if (!policy_cache) return E_MALLOC;

    // 遍历 policyids
    for (size_t i = 0; i < policyids.size(); i++) {
	int policyid = policyids[i];

	map<string, adx_size_t> dsp_policy_control_info;
	errcode = adx_redis_command(conn, dsp_policy_control_info, "HGETALL dsp_policy_control_%d", policyid); // policy control 信息
	if (errcode == E_SUCCESS) {

	    adx_cache_t *policy_child_policyid = adx_cache_add(policy_cache, CACHE_NUM(policyid), CACHE_NULL());

	    map<string, adx_size_t>::iterator iter; 
	    for(iter = dsp_policy_control_info.begin(); iter != dsp_policy_control_info.end(); iter++) {

		adx_cache_add(policy_child_policyid,
			CACHE_STR(iter->first.c_str()),
			CACHE_NUM(iter->second));
	    }
	}
    }

    // adx_cache_display(policy_cache);
    return E_SUCCESS;
}

// redis数据结构 dsp_policy_info_(policyid) 获取 campaign_id
int adx_dump_load_campaign_id(redisContext *conn, int policyid, int &campaign_id)
{
    int errcode = E_SUCCESS;

    string json_value;
    // errcode = redis_get_string(conn, "dsp_policy_info_" + intToString(policyid), json_value);
    errcode = adx_redis_command(conn, json_value, "GET dsp_policy_info_%d", policyid);
    if (errcode != E_SUCCESS) return errcode;

    string value;
    errcode = adx_json_key_value(json_value.c_str(), "campaignid", value);
    if (errcode == E_SUCCESS) {
	campaign_id = atoi(value.c_str());
    }

    return errcode;
}

// redis数据结构 dsp_mapid_(mapid) 获取 creative_id
int adx_dump_load_creative_id(redisContext *conn, int mapid, int &creative_id)
{
    int errcode = E_SUCCESS;

    string json_value;
    // errcode = redis_get_string(conn, "dsp_mapid_" + intToString(mapid), json_value);
    errcode = adx_redis_command(conn, json_value, "GET dsp_mapid_%d", mapid);
    if (errcode != E_SUCCESS) return errcode;

    string value;
    errcode = adx_json_key_value(json_value.c_str(), "creativeid", value);
    if (errcode == E_SUCCESS) {
	creative_id = atoi(value.c_str());
    }

    return errcode;
}

int adx_dump_load_map(redisContext *conn, adx_cache_t *root)
{
    int errcode = E_SUCCESS;

    adx_cache_t *policy_cache = adx_cache_find(root, CACHE_STR("policy")); // find root ==> policy
    if (!policy_cache) return E_TRACK_UNDEFINE_MAPID;

    adx_cache_t *map_cache = adx_cache_add(root, CACHE_STR("map"), CACHE_NULL()); // 创建 root ==> map
    if (!map_cache) return E_MALLOC;

    adx_list_t *p = NULL;
    adx_list_for_each(p, &policy_cache->child_list) { // 遍历 root ==> policy
	adx_cache_t *node = adx_list_entry(p, adx_cache_t, next_list);
	int policyid = adx_cache_value_to_number(node->key);

	int campaignid = 0;
	errcode = adx_dump_load_campaign_id(conn, policyid, campaignid);
	if (errcode != E_SUCCESS) adx_log(LOGERROR, "adx_dump : errcode=0x%X policyid=%d campaignid=%d", errcode, policyid, campaignid);
	if (errcode == E_SUCCESS) {
	    adx_cache_add(node, CACHE_STR("campaignid"), CACHE_NUM(campaignid));
	}

	vector<adx_size_t> mapids;
	// if (errcode == E_SUCCESS) errcode = redis_smembers_value_number(conn, "dsp_policy_mapids_" + intToString(policyid), mapids); // 全部 mapdi
	if (errcode == E_SUCCESS) errcode = adx_redis_command(conn, mapids, "SMEMBERS dsp_policy_mapids_%d", policyid);
	if (errcode == E_SUCCESS) {

	    for (size_t i = 0; i < mapids.size(); i++) {
		int mapid = mapids[i];

		int creativeid = 0;
		errcode = adx_dump_load_creative_id(conn, mapid, creativeid); //获取 creative_id
		if (errcode != E_SUCCESS) adx_log(LOGERROR, "adx_dump : errcode=0x%X mapid=%d creativeid=%d", errcode, mapid, creativeid);

		adx_cache_t *mapid_cache = adx_cache_add(map_cache, CACHE_NUM(mapid), CACHE_NULL());
		adx_cache_add(mapid_cache, CACHE_STR("campaignid"), CACHE_NUM(campaignid));
		adx_cache_add(mapid_cache, CACHE_STR("policyid"), CACHE_NUM(policyid));
		adx_cache_add(mapid_cache, CACHE_STR("creativeid"), CACHE_NUM(creativeid));
	    }
	}
    }

    return E_SUCCESS;
}

// 检测 KPI 是否有变化
void adx_dump_check_kpi(adx_cache_t *new_root, adx_dump_redis_conn *conn)
{
    int errcode = E_SUCCESS;
    char TIME_MONTH_DAY[32];
    get_time_str(6, NULL, TIME_MONTH_DAY);

    adx_cache_t *old_campaign_all = adx_cache_find(get_adx_cache_root(), CACHE_STR("campaign")); // find old campaign
    adx_cache_t *new_campaign_all = adx_cache_find(new_root, CACHE_STR("campaign")); // find new campaign
    if (old_campaign_all && new_campaign_all) {

	adx_list_t *p = NULL;
	adx_list_for_each(p, &old_campaign_all->child_list) { // 遍历 old campaign

	    adx_cache_t *old_campaign_control = adx_list_entry(p, adx_cache_t, next_list); // old control
	    adx_size_t campaignid = adx_cache_value_to_number(old_campaign_control->key);
	    adx_cache_t *new_campaign_control = adx_cache_find(new_campaign_all, CACHE_NUM(campaignid)); // new control

	    adx_size_t old_dayimp_day = adx_cache_to_number(adx_cache_find_str(old_campaign_control, "dayimp_%s", TIME_MONTH_DAY));
	    adx_size_t old_dayclk_day = adx_cache_to_number(adx_cache_find_str(old_campaign_control, "dayclk_%s", TIME_MONTH_DAY));
	    adx_size_t old_daybudget_day = adx_cache_to_number(adx_cache_find_str(old_campaign_control, "daybudget_%s", TIME_MONTH_DAY));

	    adx_size_t new_dayimp_day = adx_cache_to_number(adx_cache_find_str(new_campaign_control, "dayimp_%s", TIME_MONTH_DAY));
	    adx_size_t new_dayclk_day = adx_cache_to_number(adx_cache_find_str(new_campaign_control, "dayclk_%s", TIME_MONTH_DAY));
	    adx_size_t new_daybudget_day = adx_cache_to_number(adx_cache_find_str(new_campaign_control, "daybudget_%s", TIME_MONTH_DAY));

	    if (old_dayimp_day || old_dayclk_day || old_daybudget_day)
		if (new_dayimp_day || new_dayclk_day || new_daybudget_day)
		    if (old_dayimp_day < new_dayimp_day || old_dayclk_day < new_dayclk_day || old_daybudget_day < new_daybudget_day)
			if (old_dayimp_day != new_dayimp_day || old_dayclk_day != new_dayclk_day || old_daybudget_day != new_daybudget_day) {

			    adx_size_t dspcounter_imp_day = 0;
			    errcode = adx_redis_cluster_command(conn->adx_counter, dspcounter_imp_day, "HGET dspcounter_%ld d_imp_c_%s", campaignid, TIME_MONTH_DAY);

			    adx_size_t dspcounter_clk_day = 0;
			    errcode = adx_redis_cluster_command(conn->adx_counter, dspcounter_clk_day, "HGET dspcounter_%ld d_clk_c_%s", campaignid, TIME_MONTH_DAY);

			    adx_size_t dspcounter_cost_day = 0;
			    errcode = adx_redis_cluster_command(conn->adx_counter, dspcounter_cost_day , "HGET dspcounter_%ld d_cost_c_%s", campaignid, TIME_MONTH_DAY);

			    adx_log(LOGERROR, "adx_dump : check_kpi campaignid=%ld old_dayimp_day=%ld old_dayclk_day=%ld old_daybudget_day=%ld",
				    campaignid, old_dayclk_day, old_dayclk_day, old_daybudget_day);
			    adx_log(LOGERROR, "adx_dump : check_kpi campaignid=%ld new_dayimp_day=%ld new_dayclk_day=%ld new_daybudget_day=%ld",
				    campaignid, new_dayimp_day, new_dayclk_day, new_daybudget_day);
			    adx_log(LOGERROR, "adx_dump : check_kpi campaignid=%ld dspcounter_imp_day=%ld dspcounter_clk_day=%ld dspcounter_cost_day=%ld",
				    campaignid, dspcounter_imp_day, dspcounter_clk_day, dspcounter_cost_day);

			    // 暂停状态
			    int campaign_pause = 0;
			    adx_size_t dayimp_day = new_dayimp_day ? new_dayimp_day : -1;
			    adx_size_t dayclk_day = new_dayclk_day ? new_dayclk_day : -1;
			    if (dayimp_day == -1 && dayclk_day == -1) { // 不限

			    } else if (dayimp_day == -1) { // 限制点击
				if (dspcounter_clk_day >= dayclk_day)
				    campaign_pause = 11;

			    } else if (dayclk_day == -1) { // 限制展现
				if (dspcounter_imp_day >= dayimp_day)
				    campaign_pause = 11;

			    } else { // 同时限制
				if (dspcounter_imp_day >= dayimp_day && dspcounter_clk_day >= dayclk_day)
				    campaign_pause = 11;
			    }

			    new_daybudget_day = new_daybudget_day ? new_daybudget_day : -1;
			    if (new_daybudget_day != -1 && dspcounter_cost_day / 1000 >= new_daybudget_day * 100) 
				campaign_pause = 12;

			    adx_log(LOGERROR, "adx_dump : check_kpi campaignid=%ld campaign_pause=%d", campaignid, campaign_pause);

			    if (campaign_pause) {
				errcode = adx_redis_command(conn->adx_controller, "setex pause_campaign_%lu %d %d", campaignid, get_time_sec_today_end(), campaign_pause);
			    } else {
				errcode = adx_redis_command(conn->adx_controller, "del pause_campaign_%ld", campaignid);
			    }
			}
	}
    }

    adx_cache_t *old_policy_all = adx_cache_find(get_adx_cache_root(), CACHE_STR("policy")); // find old policy
    adx_cache_t *new_policy_all = adx_cache_find(new_root, CACHE_STR("policy")); // find new policy
    if (old_policy_all && new_policy_all) {

	adx_list_t *p = NULL;
	adx_list_for_each(p, &old_policy_all->child_list) { // 遍历 old policy

	    adx_cache_t *old_policy_control = adx_list_entry(p, adx_cache_t, next_list); // old control
	    adx_size_t policyid = adx_cache_value_to_number(old_policy_control->key);
	    adx_size_t campaignid = GET_DUMP_VALUE_INT(old_policy_control, "campaignid"); // 获取policyid 对应的 campaignid
	    adx_cache_t *new_policy_control = adx_cache_find(new_policy_all, CACHE_NUM(policyid)); // new control

	    adx_size_t old_dayimp_day = adx_cache_to_number(adx_cache_find_str(old_policy_control, "dayimp_%s", TIME_MONTH_DAY));
	    adx_size_t old_dayclk_day = adx_cache_to_number(adx_cache_find_str(old_policy_control, "dayclk_%s", TIME_MONTH_DAY));
	    adx_size_t old_daybudget_day = adx_cache_to_number(adx_cache_find_str(old_policy_control, "daybudget_%s", TIME_MONTH_DAY));

	    adx_size_t new_dayimp_day = adx_cache_to_number(adx_cache_find_str(new_policy_control, "dayimp_%s", TIME_MONTH_DAY));
	    adx_size_t new_dayclk_day = adx_cache_to_number(adx_cache_find_str(new_policy_control, "dayclk_%s", TIME_MONTH_DAY));
	    adx_size_t new_daybudget_day = adx_cache_to_number(adx_cache_find_str(new_policy_control, "daybudget_%s", TIME_MONTH_DAY));

	    if (old_dayimp_day || old_dayclk_day || old_daybudget_day)
		if (new_dayimp_day || new_dayclk_day || new_daybudget_day)
		    if (old_dayimp_day < new_dayimp_day || old_dayclk_day < new_dayclk_day || old_daybudget_day < new_daybudget_day)
			if (old_dayimp_day != new_dayimp_day || old_dayclk_day != new_dayclk_day || old_daybudget_day != new_daybudget_day) {

			    adx_size_t dspcounter_imp_day = 0;
			    errcode = adx_redis_cluster_command(conn->adx_counter, dspcounter_imp_day, "HGET dspcounter_%ld d_imp_p_%ld_%s", campaignid, policyid, TIME_MONTH_DAY);

			    adx_size_t dspcounter_clk_day = 0;
			    errcode = adx_redis_cluster_command(conn->adx_counter, dspcounter_clk_day, "HGET dspcounter_%ld d_clk_p_%ld_%s", campaignid, policyid, TIME_MONTH_DAY);

			    adx_size_t dspcounter_cost_day = 0;
			    errcode = adx_redis_cluster_command(conn->adx_counter, dspcounter_cost_day , "HGET dspcounter_%ld d_cost_p_%ld_%s", campaignid, policyid, TIME_MONTH_DAY);

			    adx_log(LOGERROR, "adx_dump : check_kpi policyid=%ld old_dayimp_day=%ld old_dayclk_day=%ld old_daybudget_day=%ld",
				    policyid, old_dayclk_day, old_dayclk_day, old_daybudget_day);
			    adx_log(LOGERROR, "adx_dump : check_kpi policyid=%ld new_dayimp_day=%ld new_dayclk_day=%ld new_daybudget_day=%ld",
				    policyid, new_dayimp_day, new_dayclk_day, new_daybudget_day);
			    adx_log(LOGERROR, "adx_dump : check_kpi policyid=%ld dspcounter_imp_day=%ld dspcounter_clk_day=%ld dspcounter_cost_day=%ld",
				    policyid, dspcounter_imp_day, dspcounter_clk_day, dspcounter_cost_day);

			    // 暂停状态
			    // KPI
			    int policy_pause = 0;
			    adx_size_t dayimp_day = new_dayimp_day ? new_dayimp_day : -1;
			    adx_size_t dayclk_day = new_dayclk_day ? new_dayclk_day : -1;
			    if (dayimp_day == -1 && dayclk_day == -1) { // 不限

			    } else if (dayimp_day == -1) { // 限制点击
				if (dspcounter_clk_day >= dayclk_day)
				    policy_pause = 21;

			    } else if (dayclk_day == -1) { // 限制展现
				if (dspcounter_imp_day >= dayimp_day)
				    policy_pause = 21;

			    } else { // 同时限制
				if (dspcounter_imp_day >= dayimp_day && dspcounter_clk_day >= dayclk_day)
				    policy_pause = 21;
			    }

			    // 暂停状态 预算
			    new_daybudget_day = new_daybudget_day ? new_daybudget_day : -1;
			    if (new_daybudget_day != -1 && dspcounter_cost_day / 1000 >= new_daybudget_day * 100)
				policy_pause = 22;

			    adx_log(LOGERROR, "adx_dump : check_kpi policyid=%ld policy_pause=%d", policyid, policy_pause);

			    if (policy_pause) {
				errcode = adx_redis_command(conn->adx_controller, "setex pause_policy_%lu %d %d", policyid, get_time_sec_today_end(), policy_pause);
			    } else {
				errcode = adx_redis_command(conn->adx_controller, "del pause_policy_%ld", policyid);
			    }
			}
	}
    }
}

adx_cache_t *adx_dump_load(adx_dump_redis_conn *conn)
{
    adx_cache_t *root = adx_cache_create();
    if (!root) return NULL;

    int err_campaign = adx_dump_load_campaign_control(conn->adx_setting, root);
    int err_policy = adx_dump_load_policy_control(conn->adx_setting, root);
    int err_map = adx_dump_load_map(conn->adx_setting, root);
    if (err_campaign != E_SUCCESS || err_policy != E_SUCCESS || err_map != E_SUCCESS) {
	adx_log(LOGERROR, "adx_dump : load_campaign_control=0x%X load_policy_control=0x%X load_map=0x%X", err_campaign, err_policy, err_map);
	adx_cache_free(root);
	return NULL;
    }

    // adx_cache_display(root);
    adx_dump_check_kpi(root, conn); // 检测 KPI 是否有变化

    // adx_cache_display(root);
    return root;
}

void *adx_dump_reload(void *arg)
{

    pthread_detach(pthread_self());
    adx_dump_redis_conn *conn = (adx_dump_redis_conn *)arg;

    while(get_run_flag()) {

	adx_cache_t *old_cache = NULL;
	adx_cache_t *new_cache = adx_dump_load(conn);
	if (new_cache) {

	    old_cache = cache_root;	
	    cache_root = new_cache;
	}

	sleep(5);
	if (old_cache) adx_cache_free(old_cache);
    }

    pthread_exit(NULL);
}

int adx_dump_init(adx_dsp_conf_t *conf)
{
    // redis 物料
    redisContext *adx_setting = redisConnect(conf->AdSetting_ip, conf->AdSetting_port, conf->AdSetting_pass);
    if (!adx_setting || adx_setting->err) {
	cout << "redis conn err : AdSetting" << endl;
	return E_REDIS_CONNECT_INVALID;
    }

    // redis 投放控制
    redisContext *adx_controller = redisConnect(conf->AdController_ip, conf->AdController_port, conf->AdController_pass);
    if (!adx_controller || adx_controller->err) {
	cout << "redis conn err : AdController" << endl;
	return E_REDIS_CONNECT_INVALID;
    }

    // redis集群 展现/点击/价格/匀速/KPI/频次等 
    redisClusterContext *adx_counter = redisClusterConnect(conf->AdCounter_ip, conf->AdCounter_port, conf->AdCounter_pass);
    if (!adx_counter || adx_counter->err) {
	cout << "redis conn err : AdCounter" << endl;
	return E_REDIS_CONNECT_INVALID;
    }

    adx_dump_redis_conn *conn = (adx_dump_redis_conn *)je_malloc(sizeof(adx_dump_redis_conn));
    if (!conn) {
	return E_MALLOC;
    }

    int errcode = adx_dump_check_redis(adx_setting);
    if (errcode != E_SUCCESS) {
	fprintf(stdout, "redis dump err : 0x%X\n", errcode);
	// return errcode;
    }


    conn->adx_setting = adx_setting;
    conn->adx_controller = adx_controller;
    conn->adx_counter = adx_counter;

    adx_cache_t *root = adx_cache_create();
    if (!root) return E_MALLOC;

    adx_dump_load_campaign_control(conn->adx_setting, root);
    adx_dump_load_policy_control(conn->adx_setting, root);
    adx_dump_load_map(conn->adx_setting, root);

    pthread_t tid;
    return pthread_create(&tid, NULL, adx_dump_reload, conn);
    return 0;
}

adx_cache_t *adx_dump_get_campaign_control(adx_size_t mapid)
{
    adx_size_t campaign_id = adx_cache_to_number(
	    adx_cache_find_args(get_adx_cache_root(),
		CACHE_STR("map"), 
		CACHE_NUM(mapid), 
		CACHE_STR("campaignid"), 
		CACHE_NULL())
	    );

    return adx_cache_find_args(get_adx_cache_root(),
	    CACHE_STR("campaign"),
	    CACHE_NUM(campaign_id),
	    CACHE_NULL()
	    );
}

adx_cache_t *adx_dump_get_policy_control(adx_size_t mapid)
{
    int policy_id = adx_cache_to_number(
	    adx_cache_find_args(get_adx_cache_root(),
		CACHE_STR("map"), 
		CACHE_NUM(mapid), 
		CACHE_STR("policyid"), 
		CACHE_NULL())
	    );

    return adx_cache_find_args(get_adx_cache_root(),
	    CACHE_STR("policy"),
	    CACHE_NUM(policy_id),
	    CACHE_NULL()
	    );
}

int adx_dump_get_creativeid(adx_size_t mapid)
{
    return adx_cache_to_number(
	    adx_cache_find_args(get_adx_cache_root(),
		CACHE_STR("map"), 
		CACHE_NUM(mapid), 
		CACHE_STR("creativeid"), 
		CACHE_NULL())
	    );
}

int adx_dump_get_campaignid(adx_size_t mapid)
{
    return adx_cache_to_number(
	    adx_cache_find_args(get_adx_cache_root(),
		CACHE_STR("map"),
		CACHE_NUM(mapid),
		CACHE_STR("campaignid"),
		CACHE_NULL())
	    );
}

int adx_dump_get_policyid(adx_size_t mapid)
{
    return adx_cache_to_number(
	    adx_cache_find_args(get_adx_cache_root(),
		CACHE_STR("map"),
		CACHE_NUM(mapid),
		CACHE_STR("policyid"),
		CACHE_NULL())
	    );
}

#if 0
int main(int argc, char *argv[])
{
    adx_dsp_conf_t *conf = adx_dsp_conf_load();
    if (!conf) return -1;

    adx_dump_init(conf);

    // adx_cache_display(get_adx_cache_root()); // 打印dump 结果

    adx_cache_t *campaignid = adx_cache_find_args(get_adx_cache_root(),
	    CACHE_STR("campaign"),
	    CACHE_NUM(11),
	    CACHE_NULL()
	    );

    fprintf(stdout, "==> [%lu]\n", GET_DUMP_VALUE_INT(campaignid, "daybudget_1106"));

    return 0;
}

#endif





#ifndef __ADX_DSPADS_CONTROL_H__
#define __ADX_DSPADS_CONTROL_H__

#include "tracker.h"
#include "adx_dump.h"

int get_liveflag();
void set_liveflag(int _liveflag);

int adx_dsp_control_init(adx_dsp_conf_t *conf);

typedef struct {
	redisContext			*AdController_master;	// redis 投放控制
	redisClusterContext		*AdCounter_master;	// redis集群 展现/点击/价格(累加)

} adx_dsp_control_context_t;

typedef struct {

	// 控制信息	
	adx_size_t campaignid, policyid, creativeid;
	const char *deviceidtype, *deviceid;
	adx_cache_t *campaign_control; // dsp_campaign_control_(campaignid)
	adx_cache_t *policy_control; // dsp_policy_control_(policyid)

	// 累计信息
	adx_size_t c_imp; //活动展示数总计数
	adx_size_t c_clk; //活动点击数总计数
	adx_size_t c_cost; //活动累计花费总计

	adx_size_t p_imp; //投放策略展示总计数
	adx_size_t p_clk; //投放策略点击总计数
	adx_size_t p_cost; //投放策略累计花费总计数

	adx_size_t c_imp_day; //单日活动展示计数
	adx_size_t c_clk_day; //单日活动点击计数
	adx_size_t c_cost_day; //单日活动累计花费

	adx_size_t p_imp_day; //单日投放策略展示计数
	adx_size_t p_clk_day; //单日投放策略点击计数
	adx_size_t p_cost_day; //单日投放策略累计花费

	adx_size_t p_imp_10m;  //当前10分钟时段内展示计数(匀速投放/自动出价)
	adx_size_t p_clk_10m;  //当前10分钟时段内点击计数(匀速投放/自动出价)
	adx_size_t p_cost_10m; //当前10分钟时段内费用计数(匀速投放/自动出价)

	adx_size_t fc_campaign; // 活动频次控制计数
	adx_size_t fc_policy; // 策略频次控制计数
	adx_size_t fc_creative; // 创意频次控制计数

} ADX_DSP_COUNTER_COUNT_T;

int adx_dsp_control(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info);

#endif




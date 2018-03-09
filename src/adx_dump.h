
#ifndef __ADX_DUMP_H__
#define __ADX_DUMP_H__

#include "adx_redis.h"
#include "adx_cache.h"

// adx_dump 初始化
// int adx_dump_init(adx_dsp_conf_t *conf);
extern adx_cache_t *get_adx_cache_root();

// 获取 campaign_control 数据结构
extern adx_cache_t *adx_dump_get_campaign_control(adx_size_t mapid);

// 获取 policy_control 数据结构
extern adx_cache_t *adx_dump_get_policy_control(adx_size_t mapid);

// 获取创意ID
extern int adx_dump_get_creativeid(adx_size_t mapid);

// 获取活动ID
int adx_dump_get_campaignid(adx_size_t mapid);

// 获取策略ID
int adx_dump_get_policyid(adx_size_t mapid);

#define GET_DUMP_VALUE_INT(o,k) adx_cache_to_number(adx_cache_find(o, CACHE_STR(k)))
#define GET_DUMP_VALUE_STR(o,k) adx_cache_to_string(adx_cache_find(o, CACHE_STR(k)))

#endif



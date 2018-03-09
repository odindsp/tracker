
#ifndef __ADX_TIME_H__
#define __ADX_TIME_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include <time.h>
#include "adx_type.h"
#include "adx_alloc.h"

	char *get_time_str(int type, const char *split, char *buf); //时间转字符串
	char *get_time_str_r(adx_pool_t *pool, int type, char *split); //时间转字符串
	
	int get_time_hour(); // 当前小时
	int get_time_hour_split(int min); // 按分钟分隔小时
	
	int get_time_sec_today_end();
	int get_time_sec_hour_end();
	int get_time_sec_30min_end();
	int get_time_sec_10min_end(); // 小时按10分钟分割,当前时间到达下个十分钟的秒数

	time_t get_time_month_end();
	time_t get_time_week_end();
	time_t get_time_day_end();
	time_t get_time_hour_end();

#define TIME_MONTH_END	get_time_month_end()
#define TIME_WEEK_END	get_time_week_end()
#define TIME_DAY_END	get_time_day_end()
#define TIME_HOUR_END	get_time_hour_end()

#ifdef __cplusplus
}
#endif

#endif




#include <stdio.h>
#include <stdlib.h>
#include "adx_time.h"

#define TIME_DAY_SEC	86400

/* tm_year   年:年减去1900
 * tm_mon    月:取值区间为[0,11]
 * tm_wday   周:取值区间为[0,11]
 * tm_mday   日:取值区间为[1,31]
 * tm_hour   时:取值区间为[0,23]
 * tm_min    分:取值区间为[0,59]
 * tm_sec    秒:取值区间为[0,59] 
 */

void time_display(time_t t)
{
    struct tm *tm = localtime(&t);
    fprintf(stdout, "[%04d-%02d-%02d][%02d:%02d:%02d]\n",
	    tm->tm_year + 1900, 
	    tm->tm_mon + 1, 
	    tm->tm_mday,
	    tm->tm_hour, 
	    tm->tm_min, 
	    tm->tm_sec);
}

char *get_time_str(int type, const char *split, char *buf)
{

    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    int size = 0;
    if (type & 1) { // 年
	size += sprintf(&buf[size], "%04d", tm.tm_year + 1900);
    }

    if (type & 2) { // 月
	if (split && size) size += sprintf(&buf[size], "%s", split);
	size += sprintf(&buf[size], "%02d", tm.tm_mon + 1);
    }

    if (type & 4) { // 日
	if (split && size) size += sprintf(&buf[size], "%s", split);
	size += sprintf(&buf[size], "%02d", tm.tm_mday);
    }

    if (type & 8) { // 时
	if (split && size) size += sprintf(&buf[size], "%s", split);
	size += sprintf(&buf[size], "%02d", tm.tm_hour);
    }

    if (type & 16) { // 分
	if (split && size) size += sprintf(&buf[size], "%s", split);
	size += sprintf(&buf[size], "%02d", tm.tm_min);
    }

    if (type & 32) { // 秒
	if (split && size) size += sprintf(&buf[size], "%s", split);
	size += sprintf(&buf[size], "%02d", tm.tm_sec);
    }

    return buf;
}

char *get_time_str_r(adx_pool_t *pool, int type, char *split)
{
    char *buf = adx_alloc(pool, 128);
    if (!buf) return NULL;
    return get_time_str(type, split, buf);
}

int get_time_hour()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);
    return tm.tm_hour;
}

int get_time_hour_split(int min)
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);
    return tm.tm_min / min;
}

int get_time_sec_today_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);
    return ((23 - tm.tm_hour) * 3600) + ((60 - tm.tm_min) * 60);
}

int get_time_sec_hour_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);
    return ((60 - tm.tm_min) * 60) - tm.tm_sec;
}

int get_time_sec_30min_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    if (tm.tm_min >= 30)
	return ((60 - tm.tm_min) * 60) - tm.tm_sec;
    return ((30 - tm.tm_min) * 60) - tm.tm_sec;
}

// 小时按10分钟分割,当前时间到达下个十分钟的秒数
int get_time_sec_10min_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);
    int min = tm.tm_min % 10; // 取个位数
    return (600 - (min * 60 + tm.tm_sec));
}

time_t get_time_month_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    if (tm.tm_mon == 11) {
	tm.tm_mday = 31;
	tm.tm_hour = 23;
	tm.tm_min = 59;
	tm.tm_sec = 59;
	return mktime(&tm);
    }

    tm.tm_mon++;
    tm.tm_mday = 1;
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    return mktime(&tm) - TIME_DAY_SEC;
}

time_t get_time_week_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    if (tm.tm_wday == 0) {
	tm.tm_hour = 23;
	tm.tm_min = 59;
	tm.tm_sec = 59;
	return mktime(&tm);
    }

    t = t + (7 - tm.tm_wday) * TIME_DAY_SEC;
    localtime_r(&t, &tm);
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    return mktime(&tm);
}

time_t get_time_day_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    return mktime(&tm);
}

time_t get_time_hour_end()
{
    struct tm tm;
    time_t t = time(NULL);
    localtime_r(&t, &tm);

    tm.tm_min = 59;
    tm.tm_sec = 59;
    return mktime(&tm);
}

#if 0
int main()
{
    int t = time(NULL);
    int sec = 600 - get_time_sec_10min_end();

    time_display(t);
    time_display(t - sec);
    fprintf(stdout, "%d\n", sec);

    return 0;
}
#endif

#if 0
void dis(time_t t)
{
    struct tm *tm = localtime(&t);
    fprintf(stdout, "[%04d-%02d-%02d][%02d:%02d:%02d]\n\n",
	    tm->tm_year + 1900, 
	    tm->tm_mon + 1, 
	    tm->tm_mday,
	    tm->tm_hour, 
	    tm->tm_min, 
	    tm->tm_sec);
}

int main()
{
#if 0
    char comm[128];
    sprintf(comm, "date -s \"2016-12-12 23:59:59\""); // 12月
    sprintf(comm, "date -s \"2016-01-01 00:00:00\""); // 1月

    sprintf(comm, "date -s \"2016-01-01 00:00:00\""); // 周5
    sprintf(comm, "date -s \"2016-01-03 00:00:00\""); // 周日
    sprintf(comm, "date -s \"2016-01-04 00:00:00\""); // 周1


    sprintf(comm, "date -s \"2016-01-01 00:00:00\"");  // 十分钟分隔
    sprintf(comm, "date -s \"2016-01-01 00:01:01\""); 

    sprintf(comm, "date -s \"2016-01-01 00:10:00\""); 
    sprintf(comm, "date -s \"2016-01-01 00:10:01\""); 

    sprintf(comm, "date -s \"2016-01-01 00:20:00\""); 
    sprintf(comm, "date -s \"2016-01-01 00:20:01\""); 

    sprintf(comm, "date -s \"2016-01-01 00:59:00\""); 
    sprintf(comm, "date -s \"2016-01-01 00:59:01\""); 


    system(comm);

    // dis(get_time_month_end());
    // dis(get_time_week_end());

    fprintf(stdout, "%d\n", get_time_hour_split_10m());
#endif

#if 0
    int type = 0;
    char split[] = ":";
    char buf[1024];

    type += 1 << 1;
    type += 1 << 2;

    fprintf(stdout, "%s\n", get_time_str(type, NULL, buf));
    fprintf(stdout, "%s\n", get_time_str(type, split, buf));
#endif

    dis(get_time_month_end());
    // dis(get_time_week_end());

    return 0;
}
#endif


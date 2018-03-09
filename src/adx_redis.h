
#ifndef __ADX_REDIS_H__
#define __ADX_REDIS_H__

#include "adx_type.h"
#include <hiredis/hircluster.h>
#include <iostream>
#include <vector>
#include <map>

using namespace std;

// new API
int adx_redis_command(redisContext *conn, const char *format, ...);
int adx_redis_cluster_command(redisClusterContext *conn, const char *format, ...);

// get value string
int adx_redis_command(redisContext *conn, string &value, const char *format, ...);
int adx_redis_cluster_command(redisClusterContext *conn, string &value, const char *format, ...);

// get value number
int adx_redis_command(redisContext *conn, adx_size_t &value, const char *format, ...);
int adx_redis_cluster_command(redisClusterContext *conn, adx_size_t &value, const char *format, ...);

// get array
int adx_redis_command(redisContext *conn, vector<string> &values, const char *format, ...);
int adx_redis_command(redisContext *conn, vector<adx_size_t> &values, const char *format, ...);

// get map
int adx_redis_command(redisContext *conn, map<string, string> &values, const char *format, ...);
int adx_redis_command(redisContext *conn, map<string, adx_size_t> &values, const char *format, ...);

#endif




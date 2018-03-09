
#include <stdlib.h>
#include <string.h>
#include <hiredis/hircluster.h>

#include <iostream>
#include <vector>
#include <map>

#include "adx_type.h"
#include "adx_redis.h"
#include "errorcode.h"

using namespace std;

int adx_redis_reply_check(redisReply *reply)
{
    int errcode = E_SUCCESS;
    if (!reply) return E_REDIS_COMMAND;

    switch(reply->type) {

	case REDIS_REPLY_STRING :
	case REDIS_REPLY_ARRAY :
	case REDIS_REPLY_INTEGER :
	    break;

	case REDIS_REPLY_NIL :
	    errcode = E_REDIS_KEY_NOT_EXIST; // KEY 为空
	    break;

	case REDIS_REPLY_STATUS :
	    if (reply->str && strncmp(reply->str, "OK", 2) == 0)
		break;

	case REDIS_REPLY_ERROR :
	default :
	    errcode = E_REDIS_COMMAND; // redis 指令错误
    }

    return errcode;
}

int adx_redis_reply_value(redisReply *reply, string &value)
{
    int errcode = E_SUCCESS;
    if (!reply) return E_REDIS_COMMAND;

    if (reply->type == REDIS_REPLY_NIL) {
	errcode = E_REDIS_KEY_NOT_EXIST; // KEY 为空

    } else if (reply->type ==REDIS_REPLY_STATUS && reply->type && strcmp(reply->str, "OK") == 0) {
	value = reply->str;

    } else if (reply->type == REDIS_REPLY_STRING) { // string
	value = reply->str;

    } else if (reply->type == REDIS_REPLY_INTEGER) { // number
	char buf[64];
	snprintf(buf, 64, "%lld", reply->integer);
	value = buf;

    } else {
	errcode = E_REDIS_COMMAND; // redis 指令错误
    }

    return errcode;
}

/*********************************
 * command ==> not return
 *********************************/
int adx_redis_command(redisContext *conn, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);
#if 0
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    fputc('\n', stdout);
    fflush(stdout);
    va_end(ap);
#endif
    int errcode = adx_redis_reply_check(reply);
    if (reply) freeReplyObject(reply);
    return errcode;
}

int adx_redis_cluster_command(redisClusterContext *conn, const char *format, ...)
{
    if (conn->err) {
	redisClusterReconnect(conn);
	if (conn->err) return E_REDIS_CONNECT_INVALID;
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisClustervCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_check(reply);
    if (reply) freeReplyObject(reply);
    return errcode;
}

/*********************************
 * command ==> return string
 *********************************/
int adx_redis_command(redisContext *conn, string &value, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_value(reply, value);
    if (reply) freeReplyObject(reply);
    return errcode;
}

int adx_redis_cluster_command(redisClusterContext *conn, string &value, const char *format, ...)
{
    if (conn->err) {
	redisClusterReconnect(conn);
	if (conn->err) return E_REDIS_CONNECT_INVALID;
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisClustervCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_value(reply, value);
    if (reply) freeReplyObject(reply);
    return errcode;
}

/*********************************
 * command ==> return number
 *********************************/
int adx_redis_command(redisContext *conn, adx_size_t &value, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);

    string str;
    int errcode = adx_redis_reply_value(reply, str);
    if (errcode == E_SUCCESS) value = atol(str.c_str());

    if (reply) freeReplyObject(reply);
    return errcode;
}

int adx_redis_cluster_command(redisClusterContext *conn, adx_size_t &value, const char *format, ...)
{
    if (conn->err) {
	redisClusterReconnect(conn);
	if (conn->err) return E_REDIS_CONNECT_INVALID;
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisClustervCommand(conn, format, ap);
    va_end(ap);

    string str;
    int errcode = adx_redis_reply_value(reply, str);
    if (errcode == E_SUCCESS) value = atol(str.c_str());

    if (reply) freeReplyObject(reply);
    return errcode;
}

/*********************************
 * check array and map
 *********************************/
int adx_redis_reply_array(redisReply *reply)
{
    int errcode = E_SUCCESS;
    if (!reply) return E_REDIS_COMMAND;
    if (reply->type == REDIS_REPLY_NIL || reply->elements == 0) {
	errcode = E_REDIS_KEY_NOT_EXIST;

    } else if (reply->type != REDIS_REPLY_ARRAY) {
	errcode = E_REDIS_COMMAND;
    }

    return errcode;
}

int adx_redis_command(redisContext *conn, vector<string> &values, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_array(reply);
    if (errcode == E_SUCCESS) {
	for (size_t i = 0; i < reply->elements; i++) {
	    values.push_back(reply->element[i]->str);
	}   
    }

    if (reply) freeReplyObject(reply);
    return errcode;
}

int adx_redis_command(redisContext *conn, vector<adx_size_t> &values, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_array(reply);
    if (errcode == E_SUCCESS) {
	for (size_t i = 0; i < reply->elements; i++) {
	    values.push_back(atol(reply->element[i]->str));
	}   
    }

    if (reply) freeReplyObject(reply);
    return errcode;
}

int adx_redis_command(redisContext *conn, map<string, string> &values, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_array(reply);
    if (errcode == E_SUCCESS) {
	for (size_t i = 0, n = 0; i < reply->elements / 2; i++) {
	    char *key = reply->element[n++]->str;
	    char *value = reply->element[n++]->str;
	    values[key] = value;
	}
#if 0
	map<string, string>::iterator  iter;
	for(iter = values.begin(); iter != values.end(); iter++) {
	    fprintf(stdout, "[map][%s][%s]\n", iter->first.c_str(), iter->second.c_str());
	}
#endif
    }

    if (reply) freeReplyObject(reply);
    return errcode;
}

int adx_redis_command(redisContext *conn, map<string, adx_size_t> &values, const char *format, ...)
{
    if (conn->err) { // 连接失败
	redisReconnect(conn); // 重新连接
	if (conn->err) {
	    return E_REDIS_CONNECT_INVALID; //依然失败
	}
    }

    va_list ap;
    va_start(ap,format);
    redisReply *reply = (redisReply *)redisvCommand(conn, format, ap);
    va_end(ap);

    int errcode = adx_redis_reply_array(reply);
    if (errcode == E_SUCCESS) {
	for (size_t i = 0, n = 0; i < reply->elements / 2; i++) {
	    char *key = reply->element[n++]->str;
	    adx_size_t value = atol(reply->element[n++]->str);
	    values[key] = value;
	}
    }

    if (reply) freeReplyObject(reply);
    return errcode;
}


#if 0
int main()
{
    string value;
    int errcode = E_SUCCESS;
#if 0	
    redisContext *conn = redisConnect((char *)"127.0.0.1", 6379, (char *)"123456");

    errcode = adx_redis_command(conn, value, "set %s %s", "001", "001");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_command(conn, value, "get %s", "001");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_command(conn, value, "INCRBY 002 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_command(conn, value, "INCRBY 002 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_command(conn, value, "HINCRBY 003 001 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_command(conn, value, "HINCRBY 003 001 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());
#endif

#if 0
    redisClusterContext *conn = redisClusterConnect((char *)"127.0.0.1", 7001, (char *)"123456");

    errcode = adx_redis_cluster_command(conn, value, "set %s %s", "001", "001");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_cluster_command(conn, value, "get %s", "001");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_cluster_command(conn, value, "INCRBY 002 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_cluster_command(conn, value, "INCRBY 002 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_cluster_command(conn, value, "HINCRBY 003 001 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());

    errcode = adx_redis_cluster_command(conn, value, "HINCRBY 003 001 1");
    fprintf(stdout, "[0x%X][%s]\n", errcode, value.c_str());
#endif

#if 0
    redisContext *conn = redisConnect((char *)"127.0.0.1", 6379, (char *)"123456");

    vector<int> values;
    errcode = adx_redis_array(conn, values, "SMEMBERS 001");
    fprintf(stdout, "errcode=0x%X\n", errcode);
    for (size_t i = 0; i < values.size(); i++) {
	fprintf(stdout, "==>[array][%d]\n", values[i]);
    }
#endif

#if 0
    redisContext *conn = redisConnect((char *)"127.0.0.1", 6379, (char *)"123456");

    map<string, int> values;
    errcode = adx_redis_map(conn, values, "HGETALL 002");
    fprintf(stdout, "errcode=0x%X\n", errcode);

    map<string, int>::iterator iter;
    for(iter = values.begin(); iter != values.end(); iter++) {
	fprintf(stdout, "==>[map][%s][%d]\n", iter->first.c_str(), iter->second);
    } 
#endif

    return 0;
}
#endif




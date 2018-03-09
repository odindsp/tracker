
#include <iostream>
#include <string>
#include <vector>
#include <hiredis/hircluster.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "tracker.h"
#include "adx_redis.h"
#include "adx_kafka.h"
#include "adx_flume.h"
#include "adx_string.h"
#include "adx_dsp_control.h"

#include "adx_util.h"

#if 10

using namespace std;

static pthread_mutex_t fcgi_accept_mutex = PTHREAD_MUTEX_INITIALIZER;
// static pthread_mutex_t fcgi_counts_mutex = PTHREAD_MUTEX_INITIALIZER;

// 价格单位和精度处理
double price_normalization(const double price, const int adx)
{

    switch(adx) {

	case 14:
	    return (price / 100);

	case 3:
	case 13:
	case 23:
	    return (price * 100);

	case 16:
	    return (price / 10);
    }

    return price;
}

// 价格解密
int parse_notice_price(vector<adx_price_lib_path> price_libs, int adx, string price_coding, double &price)
{
    int errcode = E_SUCCESS;
    typedef int (*func)(char *, double *);
    for (size_t i = 0; i < price_libs.size(); i++) {

	if (price_libs[i].adx == adx) {

	    typedef int (*func)(char *, double *);
	    func fc = (func)dlsym(price_libs[i].dlopen_handle, "DecodeWinningPrice");
	    if (fc) {
		errcode = fc((char *)price_coding.c_str(), &price);
		if (errcode == E_SUCCESS && price) price = price_normalization(price, adx);
		return errcode;
	    }

	    return E_DL_DECODE_FAILED;
	}
    }

    // E_DL_DECODE_FAILED // 价格解密失败
    // E_TRACK_FAILED_DECODE_PRICE // 解析价格失败
    return E_DL_DECODE_FAILED;
}

// 解析URL参数
#define GET_URL_STR(o, s1, s2) {o = adx_string_url_param_value(s1, s2, tmp);}
#define GET_URL_NUM(o, s1, s2) {o = atoi(adx_string_url_param_value(s1, s2, tmp));}
#define GET_URL_CHAR(o, s1, s2) {char *ch = adx_string_url_param_value(s1, s2, tmp); o = ch ? *ch : 0;}
int url_info_parse(adx_url_info_t *url_info, char *remoteaddr, char *requestparam, char *ua)
{

    char tmp[1024];
    GET_URL_STR(url_info->ip,		requestparam,"ip");
    GET_URL_CHAR(url_info->mtype,		requestparam,"mtype");
    GET_URL_STR(url_info->bid,		requestparam,"bid");
    GET_URL_NUM(url_info->at,		requestparam,"at");
    GET_URL_NUM(url_info->mapid,		requestparam,"mapid");
    GET_URL_STR(url_info->impid,		requestparam,"impid");
    GET_URL_STR(url_info->impt,		requestparam,"impt");
    GET_URL_STR(url_info->impm,		requestparam,"impm");
    GET_URL_STR(url_info->w,		requestparam,"w");
    GET_URL_STR(url_info->h,		requestparam,"h");
    // GET_URL_STR(url_info->deviceid,		requestparam,"deviceid");
    GET_URL_NUM(url_info->deviceidtype,	requestparam,"deviceidtype");
    GET_URL_NUM(url_info->adx,		requestparam,"adx");
    GET_URL_STR(url_info->price,		requestparam,"price");
    GET_URL_STR(url_info->curl,		requestparam,"curl");
    GET_URL_STR(url_info->appid,		requestparam,"appid");
    GET_URL_STR(url_info->nw,		requestparam,"nw");
    GET_URL_STR(url_info->os,		requestparam,"os");
    GET_URL_STR(url_info->gp,		requestparam,"gp");
    GET_URL_STR(url_info->tp,		requestparam,"tp");
    GET_URL_STR(url_info->mb,		requestparam,"mb");
    GET_URL_STR(url_info->md,		requestparam,"md");
    GET_URL_STR(url_info->op,		requestparam,"op");
    GET_URL_STR(url_info->ds,		requestparam,"ds");
    GET_URL_STR(url_info->dealid,		requestparam,"dealid");
    GET_URL_STR(url_info->advid,		requestparam,"advid");

    url_info->deviceid = adx_string_to_lower(adx_string_url_param_value(requestparam, "deviceid", tmp));
    url_info->remoteaddr = remoteaddr;

    GET_URL_STR(url_info->activation_parameter,		requestparam, "activation_parameter");

    url_info->ua = ua;

    // fprintf(stdout, "HTTP_USER_AGENT=%s\n", url_info->ua.c_str());
    return 0;
}

// 返回请求 200
int http_request_return_200(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    string send_data = "Status: 200 OK\r\n"
	"Content-Type:text/plain; charset=utf-8\r\n"
	"Connection: close\r\n\r\n"
	"OK\r\n";
#ifndef __DEBUG__
    // 返回请求
    // pthread_mutex_lock(&fcgi_counts_mutex);
    // FCGX_PutStr(send_data.data(), send_data.size(), dsp_info->request->out);
    // pthread_mutex_unlock(&fcgi_counts_mutex);
    FCGX_FPrintF(dsp_info->request->out, send_data.data());
    FCGX_Finish_r(dsp_info->request);
#endif
    return E_SUCCESS;
}

// 返回请求 302
int http_request_return_302(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    if (url_info->curl.size() >= 2047)
	return E_UNKNOWN;

    char url[2048];
    memset(url, 0, 2048);
    memcpy(url, url_info->curl.c_str(), url_info->curl.size());
    url_decode(url, strlen(url));

    string URL = url;
    URL = replaceMacro(URL, "#GP#", url_info->gp);
    URL = replaceMacro(URL, "#TP#", url_info->tp);
    URL = replaceMacro(URL, "#MB#", url_info->mb);
    URL = replaceMacro(URL, "#OP#", url_info->op);
    URL = replaceMacro(URL, "#NW#", url_info->nw);
    URL = replaceMacro(URL, "#MAPID#", intToString(url_info->mapid).c_str());

    string bid = url_info->bid;
    if (!bid.empty() && bid.find("%") == string::npos) {

	if (URL.find("admaster") != string::npos && URL.find(",h") != string::npos) {
	    URL.insert(URL.find(",h") + 2, bid.c_str());
	}

	URL = replaceMacro(URL, "#BID#", bid);
    }

    string send_data = "Location: " + URL + "\r\n\r\n";
    // cout << "=======>" << send_data << endl;
#ifndef __DEBUG__
    // 返回请求
    // pthread_mutex_lock(&fcgi_counts_mutex);
    // FCGX_PutStr(send_data.data(), send_data.size(), dsp_info->request->out);
    // pthread_mutex_unlock(&fcgi_counts_mutex);
    FCGX_FPrintF(dsp_info->request->out, send_data.data());
    FCGX_Finish_r(dsp_info->request);
#endif
    return E_SUCCESS;
}

int http_request_return(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    if (url_info->mtype == 'c' && !url_info->curl.empty())
	return http_request_return_302(context_info, url_info, dsp_info);

    return http_request_return_200(context_info, url_info, dsp_info);
}

// 合同逻辑
int adx_deal_process(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    int errcode = E_SUCCESS;

    // 查找合同 dsp_fixprice_(adxcode)_(dealid)
    string key = "dsp_fixprice_" + intToString(url_info->adx) + "_" + url_info->dealid;
    adx_log(LOGINFO, "[%c][%d][%s][0x0][adx_deal_process][START][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), key.c_str());

    map<string, string> values;
    errcode = adx_redis_command(context_info->AdSetting_slave, values, "HGETALL dsp_fixprice_%d_%s", url_info->adx, url_info->dealid.c_str());
    if (errcode != E_SUCCESS) return errcode;

    // 判断合同价格
    if (errcode == E_SUCCESS) {
	if (url_info->mtype == 'i' && values["imp"] != "-1") {
	    dsp_info->price = atof(values["imp"].c_str());

	} else if (url_info->mtype == 'c' && values["clk"] != "-1") {
	    dsp_info->price = atof(values["clk"].c_str());
	}
    }

    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_deal_process][END][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode, key.c_str());
    return errcode;
}

// 赢价
int adx_rtb_winnotice(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    // dsp_id_price_(bid)_(impid)
    string key = "dsp_id_price_" + url_info->bid + "_" + url_info->impid;
    string value = intToString(dsp_info->price);
    adx_log(LOGINFO, "[%c][%d][%s][0x0][adx_rtb_winnotice][START][%s][%f]", url_info->mtype, url_info->adx, url_info->bid.c_str(), key.c_str(), dsp_info->price);

    // 价格写入redis
    int errcode = adx_redis_cluster_command(context_info->AdCounter_master, "SETEX dsp_id_price_%s_%s 36000 %f", url_info->bid.c_str(), url_info->impid.c_str(), dsp_info->price);
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_rtb_winnotice][END][%s][%f]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode, key.c_str(), dsp_info->price);
    return errcode;
}

// 展现
int adx_rtb_impnotice(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    string key = "dsp_id_price_" + url_info->bid + "_" + url_info->impid;
    adx_log(LOGINFO, "[%c][%d][%s][0x0][adx_rtb_impnotice][START][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), key.c_str());

    // 从赢价缓存读取价格
    string value;
    int errcode = adx_redis_cluster_command(context_info->AdCounter_master, value, "GET dsp_id_price_%s_%s", url_info->bid.c_str(), url_info->impid.c_str());

    if (errcode == E_SUCCESS) {
	dsp_info->price = atof(value.c_str());
#if 0
    } else if (errcode == (int)E_REDIS_KEY_NOT_EXIST) { // 展现先到达 暂不处理
	errcode = adx_redis_cluster_command(context_info->AdCounter_master, "SETEX dsp_impnotice_%s_%s 3600 true", url_info->bid.c_str(), url_info->impid.c_str());
	adx_log(LOGINFO, "SETEX dsp_impnotice_%s_%s 3600 true", url_info->bid.c_str(), url_info->impid.c_str());
#endif
    } else {
	// E_TRACK_FAILED_DECODE_PRICE // 解析价格失败
	errcode = E_TRACK_FAILED_DECODE_PRICE;
    }

    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_rtb_impnotice][END][%s][%f]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode, key.c_str(), dsp_info->price);
    return errcode;
}

// RTB 逻辑
int adx_rtb_process(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    int errcode = E_SUCCESS;

    if (url_info->adx == 'c') return E_SUCCESS;
    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_rtb_process]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode);

    // 价格解密
    if (!url_info->price.empty()) {
	errcode = parse_notice_price(context_info->price_libs, url_info->adx, url_info->price, dsp_info->price);
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][parse_notice_price][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode, url_info->price.c_str());

    } else {
	errcode = E_DL_DECODE_FAILED; // 价格为空
    }

    // 解密成功 并且是赢价
    if (errcode == E_SUCCESS && url_info->mtype == 'w') {
	return adx_rtb_winnotice(context_info, url_info, dsp_info);
    }

    // 解密不成功 并且是展现
    if (errcode != E_SUCCESS && url_info->mtype == 'i') {
	return adx_rtb_impnotice(context_info, url_info, dsp_info);
    }

    adx_log(LOGERROR, "[%c][%d][%s][0x%X][adx_rtb_process][PRICE=ERR][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), E_TRACK_FAILED_DECODE_PRICE, url_info->price.c_str());
    return errcode;
}

// google bid 单独处理
void google_bid_parse(string &bid)
{
    char _bid[1024];
    memset(_bid, 0, 1024);
    memcpy(_bid, bid.c_str(), bid.size());
    url_decode(_bid, strlen(_bid));
    bid = _bid;
}

string adx_ua_to_base64(const char *ua)
{
    char dest[1048576];
    base64_encode(ua, dest);
    return string(dest);
}

void adx_tracker_kafka_send(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info)
{
    string mess = "time=" + longToString(time(NULL) * 1000) +
	"|advid=" +		url_info->advid +
	"|ip=" +                (url_info->ip.empty() ? url_info->remoteaddr : url_info->ip) +
	"|mtype=" +             url_info->mtype + 
	"|bid=" +               url_info->bid + 
	"|at=" +                intToString(url_info->at) + 
	"|mapid=" +             intToString(url_info->mapid) + 
	"|impid=" +             url_info->impid + 
	"|impt=" +              url_info->impt + 
	"|impm=" +              url_info->impm + 
	"|w=" +                 url_info->w + 
	"|h=" +                 url_info->h + 
	"|deviceid=" +          url_info->deviceid + 
	"|deviceidtype=" +      intToString(url_info->deviceidtype) + 
	"|adx=" +               intToString(url_info->adx) + 
	"|curl=" +              url_info->curl + 
	"|appid=" +             url_info->appid + 
	"|nw=" +                url_info->nw + 
	"|os=" +                url_info->os + 
	"|gp=" +                url_info->gp + 
	"|tp=" +                url_info->tp + 
	"|mb=" +                url_info->mb + 
	"|md=" +                url_info->md + 
	"|op=" +                url_info->op + 
	"|ds=" +                url_info->ds + 
	"|dealid=" +            url_info->dealid +
	"|iv=" +            	intToString(dsp_info->iv) +
	"|ic=" +            	HexToString(dsp_info->ic);

    if (url_info->mtype != 'c') mess += "|price=" + DoubleToString(dsp_info->price);
    // adx_log(LOGINFO, "[KAFKA][%s]", mess.c_str());

    // 发送kafka
    adx_kafka_send(context_info->kafka_conn, mess.c_str(), mess.size());
    
    // 发送新Kafka
    mess += "|ua=" + adx_ua_to_base64(url_info->ua.c_str());
    adx_kafka_send(context_info->link_jump_conn, mess.c_str(), mess.size());
}

int adx_tracker(adx_context_t *context_info, adx_url_info_t *url_info, adx_dsp_info_t *dsp_info, char *remoteaddr, char *requestparam, char *ua)
{
    int errcode = E_SUCCESS;
    /********************************/
    /* URL 参数错误检测 */
    /********************************/
    url_info_parse(url_info, remoteaddr, requestparam, ua);
    if (!url_info->mtype) errcode = E_TRACK_EMPTY_MTYPE;		// mtype err
    if (!(url_info->mtype == 'w' || url_info->mtype == 'i' || url_info->mtype == 'c')) errcode = E_TRACK_UNDEFINE_MTYPE; //mtype err
    if (!url_info->adx) errcode = E_TRACK_EMPTY_ADX; // adx err
    if (url_info->adx <= 0 || url_info->adx >= 255) errcode = E_TRACK_UNDEFINE_ADX; // adx err
    if (!url_info->mapid) errcode = E_TRACK_EMPTY_MAPID; // mapid err
    if (errcode != E_SUCCESS) {
	adx_log(LOGERROR, "[0x%X][%d][adx_tracker][requestparam][ERR]%s", errcode, url_info->adx, requestparam);
	http_request_return(context_info, url_info, dsp_info);
	return errcode;
    }

    // bid and impid
    if (url_info->bid.empty()) errcode = E_TRACK_EMPTY_BID; // bid err
    if (url_info->impid.empty()) errcode = E_TRACK_EMPTY_IMPID; // impid err
    if (errcode != E_SUCCESS) {
	dsp_info->iv = 0; // iv标记为无效
	dsp_info->ic = errcode;	// 更新ic错误
	http_request_return(context_info, url_info, dsp_info);
	adx_log(LOGERROR, "[0x%X][%d][adx_tracker][requestparam][ERR]%s", errcode, url_info->adx, requestparam);
	adx_tracker_kafka_send(context_info, url_info, dsp_info);
	return errcode;
    }

    // google bid 单独处理
    if (url_info->adx == 16) google_bid_parse(url_info->bid);

    /**********************************/
    /* 去重 dsp_id_flag_(bid)_(impid) */
    /**********************************/
    adx_size_t _dsp_id_flag = 0;
    string key = "dsp_id_flag_" + url_info->bid + "_" + url_info->impid + "_" + url_info->advid;
    adx_log(LOGINFO, "[%c][%d][%s][0x0][adx_tracker][FLAG:START][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), key.c_str());

    // 读取 redis
    // 二进制标志位，从低到高第一位为1表示有竞价请求，第二位为1表示有结算价格信息，第三位为1表示有展现信息，第四位为1表示有点击信息
    errcode = adx_redis_cluster_command(context_info->AdCounter_master, _dsp_id_flag, "GET dsp_id_flag_%s_%s_%s",
	    url_info->bid.c_str(),
	    url_info->impid.c_str(),
	    url_info->advid.c_str());

    int dsp_id_flag = _dsp_id_flag;
    if (errcode == E_SUCCESS) {
	if (url_info->mtype == 'w') {

	    if (!(dsp_id_flag & DSP_FLAG_PRICE)) {
		errcode = adx_redis_cluster_command(context_info->AdCounter_master, "SETEX %s 36000 %d", key.c_str(), dsp_id_flag | DSP_FLAG_PRICE);

	    } else {
		errcode = E_TRACK_REPEATED_WINNOTICE; // w重复
	    }

	} else if (url_info->mtype == 'i') {
	    if (!(dsp_id_flag & DSP_FLAG_SHOW)) {
		errcode = adx_redis_cluster_command(context_info->AdCounter_master, "SETEX %s 36000 %d", key.c_str(), dsp_id_flag | DSP_FLAG_SHOW);

	    } else {
		errcode = E_TRACK_REPEATED_CLKNOTICE; // i重复
	    }

	} else if (url_info->mtype == 'c') {
	    if (dsp_id_flag & DSP_FLAG_SHOW && !(dsp_id_flag & DSP_FLAG_CLICK)) {
		errcode = adx_redis_cluster_command(context_info->AdCounter_master, "SETEX %s 36000 %d", key.c_str(), dsp_id_flag | DSP_FLAG_CLICK);

	    } else {
		errcode = E_TRACK_REPEATED_CLKNOTICE; // c重复
	    }
	}
    }

    // 请求不重复并且设置flag标识位成功
    if (errcode == E_SUCCESS) {
	dsp_info->iv = 1; // iv标记为有效
	dsp_info->ic = E_SUCCESS;

    } else {
	dsp_info->iv = 0; // iv标记为无效
	dsp_info->ic = errcode;	// 更新ic错误
    }

    adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_tracker][FLAG:END][%s]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode, key.c_str());
    // dsp_info->iv = 1;

    /**********************************/
    /* 302 跳转 */
    /**********************************/
    http_request_return(context_info, url_info, dsp_info);

    /**********************************/
    /* 价格计算 */
    /**********************************/
    if (dsp_info->iv == 1) {
	if (url_info->at == 1) {
	    /**********************************/
	    /* 定价 */
	    /**********************************/
	    errcode = adx_deal_process(context_info, url_info, dsp_info);

	} else {
	    /**********************************/
	    /* 竞价 */
	    /**********************************/
	    errcode = adx_rtb_process(context_info, url_info, dsp_info);
	}
    }

    if (context_info->conf->activation_parameter 
	    && strcmp(context_info->conf->activation_parameter,
		url_info->activation_parameter.c_str()) == 0) {
	adx_log(LOGINFO, "[%c][%d][%s][0x%X][adx_tracker][activation-URL]", url_info->mtype, url_info->adx, url_info->bid.c_str(), errcode);

    } else {
	// 发送kafka
	adx_tracker_kafka_send(context_info, url_info, dsp_info);
    }

    /**********************************/
    /* 计数流程 */
    /**********************************/
    if (url_info->mtype == 'i' || url_info->mtype == 'c') {

#ifdef __DEBUG__
	dsp_info->price = 10;
	errcode = adx_dsp_control(context_info, url_info, dsp_info);
#else
	if (dsp_info->iv == 1) errcode = adx_dsp_control(context_info, url_info, dsp_info);
#endif
    }

    /**********************************/
    /* 投放控制redis 更新时间标识 */
    /**********************************/
    set_liveflag(1);

    return errcode;
}

char *URL = (char *)
    // "adx=14&price=kVMhW18BAABmGSFHED8ND_JTyCe1Nxz9NvhxDg&mtype=w&bid=fae6e87e932c29a0c177512151114f22&at=0&mapid=31&impid=5cdef32a55397c48b8baeb3cee0c5b5c&impt=2&impm=0,0&ip=36.110.73.210&w=320&h=50&deviceid=123&deviceidtype=10&appid=9d66d9249cc5bd549b0e68b9fedc69a7&nw=1&os=1&gp=1156110000&tp=2&mb=1&md=iPhone5%2C1&op=1&dealid=&advid=123&activation_parameter=123";
    "adx=14&mtype=i&bid=fae6e87e932c29a0c177512151114f22&at=0&mapid=31&impid=5cdef32a55397c48b8baeb3cee0c5b5c&impt=2&impm=0,0&ip=36.110.73.210&w=320&h=50&deviceid=123&deviceidtype=10&appid=9d66d9249cc5bd549b0e68b9fedc69a7&nw=1&os=1&gp=1156110000&tp=2&mb=1&md=iPhone5%2C1&op=1&dealid=&advid=123";
    // "adx=14&mtype=c&bid=fae6e87e932c29a0c177512151114f22&at=0&mapid=31&impid=5cdef32a55397c48b8baeb3cee0c5b5c&impt=2&impm=0,0&ip=36.110.73.210&w=320&h=50&deviceid=123&deviceidtype=10&appid=9d66d9249cc5bd549b0e68b9fedc69a7&nw=1&os=1&gp=1156110000&tp=2&mb=1&md=iPhone5%2C1&op=1&dealid=&url=&advid=123&curl=http%3A%2F%2Fwww.baicmotorsales.com%2Fpc%2Fwap%2Fspringsales%2F";

int adx_dsp_doit(FCGX_Request *request, adx_pool_t *pool, adx_context_t *context_info)
{
    int errcode = E_SUCCESS;

    // init dsp_info
    adx_url_info_t *url_info = &context_info->url_info;
    adx_dsp_info_t *dsp_info = &context_info->dsp_info;
    dsp_info->iv = 0;
    dsp_info->ic = 0;
    dsp_info->price = 0;
    dsp_info->request = request;
    dsp_info->pool = pool;

    // 获取请求
    pthread_mutex_lock(&fcgi_accept_mutex);
#ifndef __DEBUG__
    errcode = FCGX_Accept_r(request);
#endif
    pthread_mutex_unlock(&fcgi_accept_mutex);
    if (errcode != E_SUCCESS) {
	adx_log(LOGINFO, "FCGX_Accept_r=%d", errcode);
	return errcode;
    }

#ifndef __DEBUG__
    // 获取请求参数
    char *remoteaddr = FCGX_GetParam("REMOTE_ADDR", request->envp);
    char *requestparam = FCGX_GetParam("QUERY_STRING", request->envp);
#else
    char *remoteaddr = (char *)"127.0.0.1";
    char *requestparam = URL;
#endif
    char *ua = FCGX_GetParam("HTTP_USER_AGENT", request->envp);
    if (!remoteaddr || !ua) {
	errcode = E_TRACK_INVALID_REQUEST_ADDRESS;
	http_request_return_200(context_info, url_info, dsp_info);

    } else if (!requestparam) {
	errcode = E_TRACK_INVALID_REQUEST_PARAMETERS;
	http_request_return_200(context_info, url_info, dsp_info);

    } else {
	errcode = adx_tracker(context_info, url_info, dsp_info, remoteaddr, requestparam, ua);
    }
    
    return errcode;
}

#endif



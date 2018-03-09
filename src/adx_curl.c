
#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include "adx_curl.h"

// 请求URL recv 回调
int adx_curl_recv(char *buf, int size, int nmemb, void *dest)
{
	// fwrite(buf, size, nmemb, stdout);
	return (size * nmemb);
}

// 请求URL
int adx_curl_open(const char *url)
{
	CURL *curl = curl_easy_init();
	if(curl == NULL) return -1;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, adx_curl_recv);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
	CURLcode ret = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return ret;
}





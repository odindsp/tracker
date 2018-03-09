
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "adx_util.h"

// TODO:
// 历史版本依赖函数,临时存放
using namespace std;

int replaceMacro_strtok(string &left, string &right, string delim)
{
    string str = right;
    int size = str.find(delim);
    if (size < 0) return -1;

    left = str.substr(0, size);
    right = str.substr(size + delim.size(), str.size());
    return 0;
}

string replaceMacro(string buf, string src, string dest)
{
    string res;
    string left;
    string right = buf;
    for (;;) {

	if (replaceMacro_strtok(left, right, src)) {

	    res += right;
	    return res;
	}

	res += left;
	res += dest;
    }
}

void replaceMacro(string &str, const char *macro, const char *data)
{
    if (data == NULL)
	return;
    while (str.find(macro) < str.size())
	str.replace(str.find(macro), strlen(macro), data);

    return;
}

string HexToString(int arg)
{
    char num[64];
    snprintf(num, 64, "0x%X", arg);
    return string(num);
}

string DoubleToString(double arg)
{
    char num[64];
    snprintf(num, 64, "%.2f", arg);
    return string(num);
}

string intToString(int arg)
{
    char num[64];
    snprintf(num, 64, "%d", arg);
    return string(num);
}

string longToString(long arg)
{
    char num[64];
    snprintf(num, 64, "%ld", arg);
    return string(num);
}

int adx_except_ceil(int x, int y)
{
    double X = x, Y = y;
    return ceil(X / Y); 
}

int adx_except_ceil(adx_size_t x, adx_size_t y)
{
    double X = x, Y = y;
    return ceil(X / Y); 
}




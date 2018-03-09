
#ifndef __ADX_TEMP_H__
#define __ADX_TEMP_H__

#include "adx_type.h"
#include <iostream>

using namespace std;

string HexToString(int arg);
string DoubleToString(double arg);
string intToString(int arg);
string longToString(long arg);
string replaceMacro(string buf, string src, string dest);

int adx_except_ceil(int x, int y);
int adx_except_ceil(adx_size_t x, adx_size_t y);

#endif



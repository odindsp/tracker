
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "adx_string.h"

int adx_empty(adx_str_t str)
{
    return (!str.str || !str.len) ? 1 : 0;
}

adx_str_t adx_strdup(adx_str_t str)
{
    adx_str_t dest = {0};
    if (adx_empty(str)) return dest;

    dest.str = je_malloc(str.len + 1);
    if (!dest.str) return dest;

    dest.len = str.len;
    dest.str[dest.len] = 0;
    memcpy(dest.str, str.str, str.len);
    return dest;
}

void adx_str_free(adx_str_t str)
{
    if (!adx_empty(str)) je_free(str.str);
}

adx_str_t adx_str_init(const char *s, int len)
{
    adx_str_t str;
    str.str = (char *)s;
    str.len = len;
    return str;
}

int int_to_str(int d, char *buf)
{
    return sprintf(buf, "%d", d);
}

int double_to_str(double d, char *buf)
{
    return sprintf(buf, "%f", d);
}

int hex_to_str(int x, char *buf)
{
    return sprintf(buf, "0x%X", x);
}

char *int_to_binary(adx_pool_t *pool, int num, int len)
{

    int i, size = 0, split = 0;
    char *buf = adx_alloc(pool, 128);
    if (!buf) return NULL;

    for (i = len - 1; i >= 0; i--) {

	if (num & (1 << i)) buf[size++] = '1';
	else buf[size++] = '0';

	if (++split >= 4) {
	    buf[size++] = ' ';
	    split = 0;
	}
    }	

    if (buf[size - 1] == ' ') buf[size - 1] = 0;
    buf[size] = 0;
    return buf;
}

char *adx_string_to_upper(char *buf)
{
    char *str = NULL;
    if (!buf) return NULL;
    for (str = buf; *str; str++) {
	*str = toupper(*str);
    }

    return buf;
}

char *adx_string_to_lower(char *buf)
{
    char *str = NULL;
    if (!buf) return NULL;
    for (str = buf; *str; str++) {
	*str = tolower(*str);
    }

    return buf;
}

char *adx_string_url_param_value(const char *url, const char *key, char *value)
{
    char buf[2048];
    int len = strlen(url);
    if (len >= 2048) return 0;
    strcpy(buf, url);

    char *saveptr = NULL;
    char *str = strtok_r(buf, "&", &saveptr);
    while(str) {

	char *_value = strchr(str, '=');
	if (_value) {
	    *_value = 0;
	    if (strcmp(key, str) == 0) {
		if (strlen(_value) >= 1024) return ""; // GET 长度1024
		strcpy(value, _value + 1);
		return value;
	    }
	}

	str = strtok_r(NULL, "&", &saveptr);
    }

    return "";
}

char *url_encode(char const *s, int len, int *new_length)
{
    const char *from, *end;
    from = s;
    end = s + len;
    unsigned char *start,*to;
    start = to = (unsigned char *) malloc(3 * len + 1);
    unsigned char c;
    unsigned char hexchars[] = "0123456789ABCDEF";
    while (from < end) 
    {
	c = *from++;
	if (c == ' ')
	{
	    *to++ = '+';
	}
	else if ((c < '0' && c != '-' && c != '.')||(c < 'A' && c > '9')||(c > 'Z' && c < 'a' && c != '_')||(c > 'z'))
	{
	    to[0] = '%';
	    to[1] = hexchars[c >> 4];
	    to[2] = hexchars[c & 15];
	    to += 3;
	}
	else
	{
	    *to++ = c;
	}		     
    }
    *to = 0;
    if (new_length)
    {
	*new_length = to - start;
    }
    return (char *) start;
}

int url_decode(char *str, int len)
{
    char *dest = str;
    char *data = str;
    int value;
    unsigned char  c;
    while (len--)
    {
	if (*data == '+')
	{
	    *dest = ' ';
	}
	else if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1))&& isxdigit((int) *(data + 2)))
	{
	    c = ((unsigned char *)(data+1))[0];
	    if (isupper(c))
		c = tolower(c);
	    value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;
	    c = ((unsigned char *)(data+1))[1];
	    if (isupper(c))
		c = tolower(c);
	    value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;
	    *dest = (char)value ;
	    data += 2;
	    len -= 2;
	}
	else
	{
	    *dest = *data;
	}
	++data;
	++dest;
    }
    *dest = '\0';
    return (dest - str);
}

const char base[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="; 

/* Base64 编码 */ 
char *base64_encode(const char *src, char *dest)
{ 
    int i = 0;
    int tmp = 0;
    int temp = 0;
    int prepare = 0; 
    char changed[4]; 
    int src_len = strlen(src); 

    while (tmp < src_len) {

	temp = 0; 
	prepare = 0; 
	memset(changed, '\0', 4); 
	while (temp < 3) {
	    if (tmp >= src_len) break;
	    prepare = ((prepare << 8) | (src[tmp] & 0xFF)); 
	    tmp++;
	    temp++; 
	}

	prepare = (prepare<<((3-temp)*8)); 
	for (i = 0; i < 4 ;i++ ) {

	    if (temp < i) 
		changed[i] = 0x40; 
	    else 
		changed[i] = (prepare>>((3-i)*6)) & 0x3F; 

	    int ch = changed[i];
	    *dest++ = base[ch];
	} 
    } 

    *dest = '\0'; 
    return dest;
} 

/* 转换算子 */ 
static char find_pos(char ch)   
{ 
    char *ptr = (char*)strrchr(base, ch);//the last position (the only) in base[] 
    return (ptr - base); 
} 

/* Base64 解码 */ 
char *base64_decode(const char *src, char *dest)
{ 
    int i = 0; 
    int tmp = 0; 
    int temp = 0;
    int prepare = 0; 
    int equal_count = 0; 
    int src_len = strlen(src);

    if (*(src + src_len - 1) == '=') 
	equal_count += 1; 

    if (*(src + src_len - 2) == '=') 
	equal_count += 1; 

    if (*(src + src_len - 3) == '=') 
	equal_count += 1; 

    while (tmp < (src_len - equal_count)) {

	temp = 0; 
	prepare = 0; 
	while (temp < 4) {
	    if (tmp >= (src_len - equal_count)) break; 
	    prepare = (prepare << 6) | (find_pos(src[tmp]));
	    temp++;
	    tmp++;
	}

	prepare = prepare << ((4-temp) * 6); 
	for (i = 0; i < 3; i++) {

	    if (i == temp) break;
	    *dest++ = (char)((prepare>>((2-i)*8)) & 0xFF);
	} 
    } 

    *dest = '\0'; 
    return dest; 
}




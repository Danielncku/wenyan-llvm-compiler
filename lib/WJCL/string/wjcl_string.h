#pragma once
#ifndef __WJCL_STRING_H__
#define __WJCL_STRING_H__

#include <stdint.h>
#include <string.h>

typedef char* string;

#define strLen(str) *((uint64_t*)(str - 8))
#define strEquals(str1, str2) (strLen(str1) == strLen(str2) && strcmp(str1, str2) == 0)
#define strEqualsC(str1, str2) strcmp(str1, str2) == 0
#define strToInt(str) atoll(str)
#define strToFloat(str) atof(str)
#define strFree(str) free(str - 8);

// Method
string strNew(const char* str);
string strFromFormat(const char* fmt, ...);
string strFromFormatV(const char* fmt, va_list argv);
void strAdd(string* dst, const string res);
void strAddC(string* dst, const char* res);
void strAddFormat(string* out, const char* fmt, ...);
uint32_t strHash(const char* str);
uint32_t strHashn(const char* str, uint32_t n);

// Implementation
#ifdef WJCL_STRING_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

string strNew(const char* str) {
    size_t len = strlen(str);
    string out = malloc(len + 9);
    memcpy(out + 8, str, len + 1);
    *((uint64_t*)out) = len;
    return out + 8;
}

void strAdd(string* dst, const string res) {
    if (res == NULL) return;
    uint64_t dstLen = *((uint64_t*)(*dst - 8));
    uint64_t resLen = *((uint64_t*)(res - 8));

    char* a = (*dst) - 8;
    a = realloc(a, dstLen + resLen + 9);
    memcpy(a + 8 + dstLen, res, resLen + 1);
    *((uint64_t*)a) = dstLen + resLen;
    (*dst) = a + 8;
}

void strAddC(string* dst, const char* res) {
    if (res == NULL) return;
    uint64_t dstLen = *((uint64_t*)(*dst - 8));
    uint64_t resLen = strlen(res);

    char* a = (*dst) - 8;
    a = realloc(a, dstLen + resLen + 9);
    if (a == NULL) return;
    memcpy(a + 8 + dstLen, res, resLen + 1);
    *((uint64_t*)a) = dstLen + resLen;
    (*dst) = a + 8;
}

void strAddFormat(string* out, const char* fmt, ...) {
    va_list ap, cpy;
    va_start(ap, fmt);

    char staticbuf[1024], *buf = staticbuf;
    size_t buflen = strlen(fmt) * 2;

    if (buflen > sizeof(staticbuf)) {
        buf = malloc(buflen);
        if (buf == NULL) return;
    } else
        buflen = sizeof(staticbuf);

    while (1) {
        buf[buflen - 2] = '\0';
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (buf[buflen - 2] != '\0') {
            if (buf != staticbuf) free(buf);
            buflen *= 2;
            buf = malloc(buflen);
            if (buf == NULL) return;
            continue;
        }
        break;
    }
    va_end(ap);

    strAddC(out, buf);
    if (buf != staticbuf) free(buf);
}

string strFromFormat(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    string out = strFromFormatV(fmt, ap);
    va_end(ap);
    return out;
}

string strFromFormatV(const char* fmt, va_list argv) {
    va_list cpy;

    char staticbuf[1024], *buf = staticbuf;
    size_t buflen = strlen(fmt) * 2;

    if (buflen > sizeof(staticbuf)) {
        buf = malloc(buflen);
        if (buf == NULL) return NULL;
    } else
        buflen = sizeof(staticbuf);

    while (1) {
        buf[buflen - 2] = '\0';
        va_copy(cpy, argv);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (buf[buflen - 2] != '\0') {
            if (buf != staticbuf) free(buf);
            buflen *= 2;
            buf = malloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    string out = strNew(buf);
    if (buf != staticbuf) free(buf);
    return out;
}

// BKDRHash
uint32_t strHash(const char* str) {
    uint32_t hash = 0, seed = 131;  // 31 131 1313 13131 131313 etc..
    while (*str)
        hash = hash * seed + (*str++);
    return hash;
}

uint32_t strHashn(const char* str, uint32_t n) {
    uint32_t hash = 0, seed = 131;  // 31 131 1313 13131 131313 etc..
    for (size_t i = 0; i < n; i++)
        hash = hash * seed + str[i];
    return hash;
}

#endif /* WJCL_STRING_IMPLEMENTATION */

#endif
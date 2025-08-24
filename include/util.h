#ifndef UTIL_H
#define UTIL_H
#include "common.h"

int kstrcmp(const char* a, const char* b);
int kstrncmp(const char* a, const char* b, int n);
void kstrcpy(char* dst, const char* src);
int kstrlen(const char* s);
void kstrncpy(char *dest, const char *src, int n);

#endif

#include "../include/util.h"

int kstrcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(*(unsigned char*)a) - (int)(*(unsigned char*)b);
}
int kstrncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (b[i] == 0) return 0;
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }
    return 0;
}
void kstrcpy(char* dst, const char* src) {
    while ((*dst++ = *src++));
}
int kstrlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}
void kstrncpy(char *dest, const char *src, int n) {
    int i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

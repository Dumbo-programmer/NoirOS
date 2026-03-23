#include "../include/util.h"

/* kstrcmp: standard lexicographic comparison.
 * Returns <0, 0, or >0.  Both a[i]==0 and b[i]==0 must be checked
 * simultaneously so the function correctly returns 0 only when both
 * strings terminate at the same position. */
int kstrcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(*(unsigned char*)a) - (int)(*(unsigned char*)b);
}

/* kstrncmp: compare at most n characters.
 * Returns 0 only when the first n characters of both strings match
 * OR both reach their NUL terminator simultaneously within n chars.
 * Previous version returned 0 early on b[i]==0 while a[i] might still
 * differ, masking mismatches (e.g. kstrncmp("abc","ab",3) wrongly == 0). */
int kstrncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0)  return 0; /* both NUL at same position */
    }
    return 0;
}

/* kstrcpy: unsafe unbounded copy — use only when dst is guaranteed large enough. */
void kstrcpy(char* dst, const char* src) {
    while ((*dst++ = *src++));
}

/* kstrncpy: bounded copy that always NUL-terminates.
 * Requires n > 0; copies at most n-1 characters and writes a NUL byte. */
void kstrncpy(char* dest, const char* src, int n) {
    if (n <= 0) return;          /* guard: nothing to write */
    int i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int kstrlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

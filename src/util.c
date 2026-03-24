#include "../include/util.h"

/**
 * Lexicographic string comparison.
 * @param a first NUL-terminated string
 * @param b second NUL-terminated string
 * @return <0 if a<b, 0 if equal, >0 if a>b
 */
int kstrcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (int)(*(unsigned char*)a) - (int)(*(unsigned char*)b);
}

/**
 * Compare up to `n` characters of two strings.
 * @param a first string
 * @param b second string
 * @param n maximum characters to compare
 * @return <0 if a<b, 0 if equal up to n, >0 if a>b within n
 */
int kstrncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0)  return 0; /* both NUL at same position */
    }
    return 0;
}

/**
 * Copy NUL-terminated string `src` into `dst` (no bounds checking).
 * Use only when `dst` is guaranteed to be large enough.
 */
void kstrcpy(char* dst, const char* src) {
    while ((*dst++ = *src++));
}

/**
 * Copy at most `n-1` bytes from `src` into `dest` and NUL-terminate.
 * If n<=0 the function is a no-op.
 * @param dest destination buffer
 * @param src source string
 * @param n size of destination buffer
 */
void kstrncpy(char* dest, const char* src, int n) {
    if (n <= 0) return;          /* guard: nothing to write */
    int i = 0;
    while (i < n - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Return the length of a NUL-terminated string (number of bytes before NUL).
 * @param s input string
 * @return length in bytes (excluding NUL)
 */
int kstrlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

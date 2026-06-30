#include "kstring.h"

size_t kstrlen(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') n++;
    return n;
}

int kstrcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

char* kstrcpy(char* dst, const char* src, size_t max) {
    size_t i = 0;
    if (max == 0) return dst;

    while (i < max - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return dst;
}

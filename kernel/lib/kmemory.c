#include "kmemory.h"
#include <stdint.h>

void* kmemcpy(void* dst, const void* src, size_t n) {
    uint8_t*       d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* kmemset(void* dst, int val, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    uint8_t  v = (uint8_t)val;
    for (size_t i = 0; i < n; i++) d[i] = v;
    return dst;
}

int kmemcmp(const void* a, const void* b, size_t n) {
    const uint8_t* pa = (const uint8_t*)a;
    const uint8_t* pb = (const uint8_t*)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

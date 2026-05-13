#include <stdint.h>

static inline int sys_write(const char* str, uint32_t len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "a"(1), "b"(str), "c"(len) : "memory");
    return ret;
}

static inline void sys_exit(void) {
    __asm__ volatile ("int $0x80" : : "a"(0), "b"(0));
    while(1);
}

void user_init_main(void) {
    const char msg1[] = "[init] Ring 3 real confirmado!\n";
    const char msg2[] = "[init] Terminando.\n";

    sys_write(msg1, sizeof(msg1)-1);
    sys_write(msg2, sizeof(msg2)-1);
    sys_exit();
}
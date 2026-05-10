/* user_init.c — Proceso init en ring 3
 * Todo inline — cero calls externos, cero referencias absolutas.
 * Funciona correctamente cuando se copia a cualquier dirección.
 */

#include <stdint.h>

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_GETPID 2
#define SYS_SLEEP  3

static inline int sys_write(const char* str, uint32_t len) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(str), "c"(len)
        : "memory");
    return ret;
}

static inline int sys_getpid(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "a"(SYS_GETPID));
    return ret;
}

static inline void sys_sleep(uint32_t ms) {
    __asm__ volatile ("int $0x80"
        : : "a"(SYS_SLEEP), "b"(ms));
}

static inline void sys_exit(int code) {
    __asm__ volatile ("int $0x80"
        : : "a"(SYS_EXIT), "b"(code));
    while(1) __asm__ volatile ("hlt");
}

/* Entry point — todo el código queda inline dentro de esta función */
void user_init_main(void) {
    /* print "[init] Ring 3 real!\n" */
    const char msg1[] = "[init] Ring 3 real!\n";
    uint32_t len1 = sizeof(msg1) - 1;
    sys_write(msg1, len1);

    /* print PID */
    const char msg2[] = "[init] Terminando.\n";
    sys_sleep(200);
    sys_write(msg2, sizeof(msg2) - 1);

    sys_exit(0);
    while(1);
}
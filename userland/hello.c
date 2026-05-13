/* hello.c — Primer programa de usuario real
 * Se compila como ELF independiente y se carga desde el VFS.
 * Solo usa INT 0x80 para comunicarse con el kernel.
 */

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_GETPID 2

static inline void sys_write(const char* s, int len) {
    __asm__ volatile ("int $0x80"
        : : "a"(SYS_WRITE), "b"(s), "c"(len) : "memory");
}

static inline void sys_exit(int code) {
    __asm__ volatile ("int $0x80"
        : : "a"(SYS_EXIT), "b"(code));
    while(1);
}

static inline int sys_getpid(void) {
    int ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret) : "a"(SYS_GETPID));
    return ret;
}

void _start(void) {
    const char msg[] = "Hola desde un programa ELF!\n";
    sys_write(msg, sizeof(msg) - 1);

    const char msg2[] = "Soy un ejecutable de usuario real.\n";
    sys_write(msg2, sizeof(msg2) - 1);

    sys_exit(0);
}
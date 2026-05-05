/* user_init.c — Primer proceso real en ring 3
 *
 * Este código corre completamente en ring 3.
 * NO puede llamar funciones del kernel directamente.
 * Toda comunicación con el kernel es via INT 0x80.
 *
 * Es el equivalente del proceso "init" de Linux —
 * el primer proceso de usuario que arranca el sistema.
 */

#include <stdint.h>

/* ── Syscall wrappers ────────────────────────────────────────────────────
 * En ring 3 no podemos llamar al kernel directamente.
 * Usamos INT 0x80 con:
 *   EAX = número de syscall
 *   EBX = argumento 1
 *   ECX = argumento 2
 */

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_GETPID 2
#define SYS_SLEEP  3
#define SYS_YIELD  4

static int sys_write(const char* str, uint32_t len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(str), "c"(len)
        : "memory"
    );
    return ret;
}

static int sys_getpid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
    );
    return ret;
}

static void sys_sleep(uint32_t ms) {
    __asm__ volatile (
        "int $0x80"
        : : "a"(SYS_SLEEP), "b"(ms)
    );
}

static void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        : : "a"(SYS_EXIT), "b"(code)
    );
    /* No retorna */
    while(1) __asm__ volatile ("hlt");
}

static void print(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    sys_write(s, len);
}

/* ── Entry point del proceso init ───────────────────────────────────────
 * Esta función es llamada desde jump_to_usermode via iret.
 * Corre completamente en ring 3. */
void user_init_main(void) {
    print("[init] Proceso init corriendo en ring 3!\n");
    print("[init] PID: ");

    int pid = sys_getpid();
    /* Imprimir PID manualmente (sin libc) */
    char buf[12];
    int i = 0;
    int n = pid;
    if (n == 0) { buf[i++] = '0'; }
    else { while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; } }
    /* Invertir */
    for (int a = 0, b = i-1; a < b; a++, b--) {
        char tmp = buf[a]; buf[a] = buf[b]; buf[b] = tmp;
    }
    buf[i++] = '\n'; buf[i] = '\0';
    sys_write(buf, (uint32_t)i);

    print("[init] Usando solo syscalls — ring 3 real confirmado!\n");
    print("[init] Durmiendo 500ms...\n");
    sys_sleep(500);
    print("[init] Listo. Terminando.\n");

    sys_exit(0);
}
/* kprintf.c — Formateador de salida del kernel
 *
 * Depende únicamente de terminal_putchar() para escribir.
 * No conoce VGA, no conoce el shell, no conoce ningún subsistema.
 *
 * Usamos __builtin_va_list en lugar de <stdarg.h> porque en un
 * entorno freestanding los builtins de GCC siempre están disponibles.
 */

#include "kprintf.h"
#include "terminal.h"
#include <stdint.h>
#include <stddef.h>

/* ── va_list mediante builtins de GCC ─────────────────────────────────── */
typedef __builtin_va_list va_list;
#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

/* ── Helpers internos ──────────────────────────────────────────────────── */
static void print_uint(uint32_t n, uint32_t base, int uppercase) {
    const char* digits_lower = "0123456789abcdef";
    const char* digits_upper = "0123456789ABCDEF";
    const char* digits = uppercase ? digits_upper : digits_lower;

    char buf[32];
    int  i = 0;

    if (n == 0) {
        terminal_putchar('0');
        return;
    }

    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }

    while (i > 0)
        terminal_putchar(buf[--i]);
}

static void print_int(int32_t n) {
    if (n < 0) {
        terminal_putchar('-');
        /* Evitar overflow en INT32_MIN: trabajar en uint32_t */
        print_uint((uint32_t)(-(n + 1)) + 1u, 10, 0);
    } else {
        print_uint((uint32_t)n, 10, 0);
    }
}

/* ── API pública ───────────────────────────────────────────────────────── */
void kprintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            terminal_putchar(fmt[i]);
            continue;
        }

        i++; /* avanzar al especificador */

        switch (fmt[i]) {
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                terminal_print(s);
                break;
            }
            case 'c':
                terminal_putchar((char)va_arg(ap, int));
                break;

            case 'd':
                print_int(va_arg(ap, int32_t));
                break;

            case 'u':
                print_uint(va_arg(ap, uint32_t), 10, 0);
                break;

            case 'x':
                print_uint(va_arg(ap, uint32_t), 16, 0);
                break;

            case 'X':
                print_uint(va_arg(ap, uint32_t), 16, 1);
                break;

            case '%':
                terminal_putchar('%');
                break;

            default:
                /* Especificador desconocido: imprimir literal */
                terminal_putchar('%');
                terminal_putchar(fmt[i]);
                break;
        }
    }

    va_end(ap);
}

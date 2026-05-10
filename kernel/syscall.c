/* syscall.c — Manejador de llamadas al sistema
 *
 * Los procesos de usuario no pueden llamar funciones del kernel directamente.
 * En cambio ejecutan: INT 0x80
 * El CPU salta al handler registrado en la IDT para el vector 0x80.
 * Aquí atendemos la llamada y retornamos al proceso.
 */

#include "syscall.h"
#include "idt.h"
#include "scheduler.h"
#include "timer.h"
#include <stdint.h>

/* Declaradas en kernel.c — las reutilizamos para escribir en pantalla */
extern void terminal_print(const char* str);
extern void terminal_putchar(char c);

/* Handler de INT 0x80 — recibe el frame completo del CPU */
static void syscall_handler(interrupt_frame_t* frame) {
    uint32_t syscall_num = frame->eax;
    uint32_t arg1        = frame->ebx;
    uint32_t arg2        = frame->ecx;
    uint32_t arg3        = frame->edx;

    (void)arg2; (void)arg3;

    int32_t result = -1;

    switch (syscall_num) {

        case SYS_EXIT:
            /* Marcar resultado y retornar — el kernel decide qué hacer */
            result = (int32_t)arg1;
            /* Recargar segmentos de kernel para que el shell funcione */
            __asm__ volatile (
                "mov $0x10, %%ax\n"
                "mov %%ax, %%ds\n"
                "mov %%ax, %%es\n"
                "mov %%ax, %%fs\n"
                "mov %%ax, %%gs\n"
                : : : "ax"
            );
            break;

        case SYS_WRITE:
            /* arg1 = puntero a string, arg2 = longitud
             * NOTA: en un OS real verificaríamos que el puntero
             * pertenece al espacio del proceso (no al kernel). */
            if (arg1 != 0) {
                const char* str = (const char*)arg1;
                uint32_t len = arg2;
                for (uint32_t i = 0; i < len && str[i]; i++) {
                    terminal_putchar(str[i]);
                }
                result = (int32_t)len;
            }
            break;

        case SYS_GETPID:
            result = (int32_t)process_current_pid();
            break;

        case SYS_SLEEP:
            /* arg1 = milisegundos */
            timer_sleep(arg1);
            result = 0;
            break;

        case SYS_YIELD:
            process_yield();
            result = 0;
            break;

        default:
            result = -1; /* syscall desconocida */
            break;
    }

    /* Retornar resultado en EAX */
    frame->eax = (uint32_t)result;
}

/* Registrar INT 0x80 en la IDT con DPL=3 para que ring 3 pueda llamarla */
void syscall_init(void) {
    /* Necesitamos acceso a la IDT directamente.
     * Usamos la función de bajo nivel para agregar la entrada con ring 3 acceso */
    extern void idt_set_gate_user(uint8_t num, uint32_t base);
    idt_set_gate_user(0x80, (uint32_t)syscall_handler);
}
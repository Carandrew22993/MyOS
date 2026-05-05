#ifndef TSS_H
#define TSS_H

#include <stdint.h>

/* El TSS (Task State Segment) es una estructura que el CPU x86 usa
 * para saber a qué stack cambiar cuando ocurre una interrupción
 * mientras se está en ring 3.
 *
 * Cuando un proceso de usuario hace una syscall o recibe una IRQ,
 * el CPU necesita cambiar al stack del kernel. El TSS le dice dónde está.
 *
 * Solo necesitamos un TSS global — no uno por proceso.
 * Actualizamos ss0/esp0 al hacer context switch. */

typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;      /* stack del kernel — actualizar en cada context switch */
    uint32_t ss0;       /* selector de segmento del kernel (0x10) */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

void tss_init(void);
void tss_set_kernel_stack(uint32_t stack_top);

#endif
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Cada entrada de la IDT ocupa 8 bytes:
 *
 *  63      48 47  44 43  40 39  32 31      16 15       0
 * ┌──────────┬──────┬──────┬──────┬──────────┬──────────┐
 * │offset31:16│present│DPL │ type │ selector │offset15:0│
 * └──────────┴──────┴──────┴──────┴──────────┴──────────┘
 */
typedef struct {
    uint16_t offset_low;   /* bits 0-15 de la dirección del handler */
    uint16_t selector;     /* selector de segmento de código (0x08 = kernel) */
    uint8_t  zero;         /* siempre 0 */
    uint8_t  type_attr;    /* tipo + atributos (presente, privilegio, tipo) */
    uint16_t offset_high;  /* bits 16-31 de la dirección del handler */
} __attribute__((packed)) idt_entry_t;

/* Puntero que se le pasa a la instrucción LIDT */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/* Estructura que el CPU empuja al stack antes de llamar al handler.
 * Nuestro stub en ASM la construye de forma consistente para todas las IRQs. */
typedef struct {
    uint32_t ds;                            /* segmento de datos */
    uint32_t edi, esi, ebp, esp_dummy;      /* pushad */
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_no, err_code;              /* número de interrupción y código de error */
    uint32_t eip, cs, eflags, useresp, ss; /* empujados por el CPU automáticamente */
} interrupt_frame_t;

void idt_init(void);
void irq_register(uint8_t irq, void (*handler)(interrupt_frame_t*));

#endif
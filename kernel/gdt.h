#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* Cada entrada de la GDT ocupa 8 bytes con este layout:
 *
 *  63       56 55    52 51   48 47      40 39      16 15       0
 * ┌──────────┬────────┬───────┬──────────┬──────────┬──────────┐
 * │ base 31:24│ flags │lim19:16│  access  │ base23:0 │ limit15:0│
 * └──────────┴────────┴───────┴──────────┴──────────┴──────────┘
 */
typedef struct {
    uint16_t limit_low;    /* bits 0-15 del límite */
    uint16_t base_low;     /* bits 0-15 de la base */
    uint8_t  base_mid;     /* bits 16-23 de la base */
    uint8_t  access;       /* tipo, privilegio, presente */
    uint8_t  granularity;  /* flags + bits 16-19 del límite */
    uint8_t  base_high;    /* bits 24-31 de la base */
} __attribute__((packed)) gdt_entry_t;

/* Puntero que se le pasa a la instrucción LGDT */
typedef struct {
    uint16_t limit;        /* tamaño de la GDT - 1 */
    uint32_t base;         /* dirección de la GDT */
} __attribute__((packed)) gdt_ptr_t;

void gdt_init(void);

#endif
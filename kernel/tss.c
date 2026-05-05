/* tss.c — Task State Segment
 *
 * Necesitamos ampliar la GDT con dos entradas nuevas:
 *   3 → segmento de código de usuario  (ring 3, ejecutable)
 *   4 → segmento de datos de usuario   (ring 3, lectura/escritura)
 *   5 → TSS                            (descriptor de sistema)
 *
 * Los selectores de segmento x86 codifican el RPL (Requested Privilege Level)
 * en los bits 0-1:
 *   0x08 = GDT entrada 1, RPL 0 → código kernel
 *   0x10 = GDT entrada 2, RPL 0 → datos kernel
 *   0x1B = GDT entrada 3, RPL 3 → código usuario  (0x18 | 3)
 *   0x23 = GDT entrada 4, RPL 3 → datos usuario   (0x20 | 3)
 *   0x28 = GDT entrada 5, RPL 0 → TSS
 */

#include "tss.h"
#include "gdt.h"
#include <stdint.h>
#include <stddef.h>

extern void gdt_flush(uint32_t);

/* GDT completa con 6 entradas */
static gdt_entry_t gdt[6];
static gdt_ptr_t   gdt_ptr;
static tss_t       tss;

#define GDT_PRESENT    (1 << 7)
#define GDT_RING0      (0 << 5)
#define GDT_RING3      (3 << 5)
#define GDT_DESCRIPTOR (1 << 4)
#define GDT_EXECUTABLE (1 << 3)
#define GDT_READWRITE  (1 << 1)
#define GDT_ACCESSED   (1 << 0)
#define GDT_GRANULARITY (1 << 7)
#define GDT_32BIT       (1 << 6)

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = base & 0xFFFF;
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;
    gdt[idx].limit_low   = limit & 0xFFFF;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].access      = access;
}

static void tss_set_entry(int idx, uint32_t base, uint32_t limit) {
    gdt[idx].limit_low   = limit & 0xFFFF;
    gdt[idx].base_low    = base & 0xFFFF;
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].access      = 0x89; /* presente, ring 0, tipo TSS disponible (32-bit) */
    gdt[idx].granularity = ((limit >> 16) & 0x0F);
    gdt[idx].base_high   = (base >> 24) & 0xFF;
}

/* Cargar el registro TR (Task Register) con el selector del TSS */
static void tss_load(uint16_t selector) {
    __asm__ volatile ("ltr %0" : : "r"(selector));
}

void tss_init(void) {
    /* 0: Null */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Código kernel — ring 0 */
    gdt_set_entry(1, 0, 0xFFFFFFFF,
        GDT_PRESENT | GDT_RING0 | GDT_DESCRIPTOR | GDT_EXECUTABLE | GDT_READWRITE,
        GDT_GRANULARITY | GDT_32BIT);

    /* 2: Datos kernel — ring 0 */
    gdt_set_entry(2, 0, 0xFFFFFFFF,
        GDT_PRESENT | GDT_RING0 | GDT_DESCRIPTOR | GDT_READWRITE,
        GDT_GRANULARITY | GDT_32BIT);

    /* 3: Código usuario — ring 3 */
    gdt_set_entry(3, 0, 0xFFFFFFFF,
        GDT_PRESENT | GDT_RING3 | GDT_DESCRIPTOR | GDT_EXECUTABLE | GDT_READWRITE,
        GDT_GRANULARITY | GDT_32BIT);

    /* 4: Datos usuario — ring 3 */
    gdt_set_entry(4, 0, 0xFFFFFFFF,
        GDT_PRESENT | GDT_RING3 | GDT_DESCRIPTOR | GDT_READWRITE,
        GDT_GRANULARITY | GDT_32BIT);

    /* 5: TSS */
    /* Limpiar el TSS */
    uint8_t* p = (uint8_t*)&tss;
    for (size_t i = 0; i < sizeof(tss_t); i++) p[i] = 0;

    tss.ss0        = 0x10;           /* selector de datos kernel */
    tss.esp0       = 0;              /* se actualiza en cada context switch */
    tss.iomap_base = sizeof(tss_t);  /* sin mapa de I/O */

    tss_set_entry(5, (uint32_t)&tss, sizeof(tss_t) - 1);

    /* Recargar la GDT con las 6 entradas */
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;
    gdt_flush((uint32_t)&gdt_ptr);

    /* Cargar el TSS */
    tss_load(0x28); /* selector entrada 5: 5 × 8 = 0x28 */
}

/* Actualizar el stack del kernel en el TSS antes de cada context switch */
void tss_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}
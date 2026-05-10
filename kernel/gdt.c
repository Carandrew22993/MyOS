/* gdt.c — Global Descriptor Table completa (6 entradas)
 *
 * Una sola GDT con todo lo necesario:
 *   0: Null
 *   1: Código kernel  (ring 0) → selector 0x08
 *   2: Datos kernel   (ring 0) → selector 0x10
 *   3: Código usuario (ring 3) → selector 0x1B
 *   4: Datos usuario  (ring 3) → selector 0x23
 *   5: TSS            (ring 0) → selector 0x28
 */

#include "gdt.h"
#include <stdint.h>
#include <stddef.h>

#define GDT_ENTRIES 6

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* TSS — definido aquí para tener todo en un lugar */
typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed)) tss_entry_t;

static tss_entry_t tss;

/* Bytes de acceso */
#define AB_PRESENT    0x80
#define AB_RING0      0x00
#define AB_RING3      0x60
#define AB_DESCRIPTOR 0x10
#define AB_EXECUTABLE 0x08
#define AB_READWRITE  0x02
#define AB_ACCESSED   0x01
/* Granularidad */
#define GB_4K         0x80
#define GB_32BIT      0x40

static void set_entry(int i, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

static void set_tss_entry(int i, uint32_t base, uint32_t limit) {
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = 0x89; /* presente, ring 0, TSS32 disponible */
    gdt[i].granularity = (limit >> 16) & 0x0F;
    gdt[i].base_high   = (base >> 24) & 0xFF;
}

extern void gdt_flush(uint32_t);

static void tss_load(void) {
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)0x28));
}

void gdt_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}

void gdt_init(void) {
    /* 0: Null */
    set_entry(0, 0, 0, 0, 0);

    /* 1: Código kernel ring 0 */
    set_entry(1, 0, 0xFFFFFFFF,
        AB_PRESENT | AB_RING0 | AB_DESCRIPTOR | AB_EXECUTABLE | AB_READWRITE,
        GB_4K | GB_32BIT);

    /* 2: Datos kernel ring 0 */
    set_entry(2, 0, 0xFFFFFFFF,
        AB_PRESENT | AB_RING0 | AB_DESCRIPTOR | AB_READWRITE,
        GB_4K | GB_32BIT);

    /* 3: Código usuario ring 3 */
    set_entry(3, 0, 0xFFFFFFFF,
        AB_PRESENT | AB_RING3 | AB_DESCRIPTOR | AB_EXECUTABLE | AB_READWRITE,
        GB_4K | GB_32BIT);

    /* 4: Datos usuario ring 3 */
    set_entry(4, 0, 0xFFFFFFFF,
        AB_PRESENT | AB_RING3 | AB_DESCRIPTOR | AB_READWRITE,
        GB_4K | GB_32BIT);

    /* 5: TSS */
    uint8_t* tp = (uint8_t*)&tss;
    for (size_t i = 0; i < sizeof(tss_entry_t); i++) tp[i] = 0;
    tss.ss0        = 0x10;
    tss.esp0       = 0;
    tss.iomap_base = sizeof(tss_entry_t);
    set_tss_entry(5, (uint32_t)&tss, sizeof(tss_entry_t) - 1);

    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_flush((uint32_t)&gdt_ptr);
    tss_load();
}
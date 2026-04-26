/* gdt.c — Global Descriptor Table
 *
 * La GDT le dice al CPU cómo dividir el espacio de memoria en segmentos
 * y qué nivel de privilegio tiene cada uno (ring 0 = kernel, ring 3 = usuario).
 *
 * Nuestra GDT tiene 3 entradas:
 *   0 → Null descriptor     (requerido por la arquitectura x86)
 *   1 → Segmento de código  (ring 0, ejecutable)
 *   2 → Segmento de datos   (ring 0, lectura/escritura)
 */

#include "gdt.h"

#define GDT_ENTRIES 3

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;

/* Bits del byte de acceso */
#define GDT_PRESENT    (1 << 7)  /* segmento presente en memoria */
#define GDT_RING0      (0 << 5)  /* privilegio kernel (ring 0) */
#define GDT_RING3      (3 << 5)  /* privilegio usuario (ring 3) */
#define GDT_DESCRIPTOR (1 << 4)  /* segmento de código/datos (no sistema) */
#define GDT_EXECUTABLE (1 << 3)  /* segmento ejecutable (código) */
#define GDT_READWRITE  (1 << 1)  /* lectura (código) o escritura (datos) */

/* Bits de granularidad (byte alto) */
#define GDT_GRANULARITY (1 << 7) /* límite en páginas de 4KB (no bytes) */
#define GDT_32BIT       (1 << 6) /* segmento de 32 bits */

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = base & 0xFFFF;
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;

    gdt[idx].limit_low   = limit & 0xFFFF;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt[idx].access      = access;
}

/* gdt_flush está en gdt_asm.asm — carga el puntero con LGDT
 * y recarga los registros de segmento */
extern void gdt_flush(uint32_t gdt_ptr_addr);

void gdt_init(void) {
    /* 0: Null descriptor — la CPU exige que la entrada 0 sea nula */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Segmento de código del kernel
     *    base=0, límite=4GB, ring 0, ejecutable, readable */
    gdt_set_entry(1,
        0x00000000,   /* base: empieza en 0 */
        0xFFFFFFFF,   /* límite: todo el espacio (con granularidad en 4KB = 4GB) */
        GDT_PRESENT | GDT_RING0 | GDT_DESCRIPTOR | GDT_EXECUTABLE | GDT_READWRITE,
        GDT_GRANULARITY | GDT_32BIT
    );

    /* 2: Segmento de datos del kernel
     *    base=0, límite=4GB, ring 0, lectura/escritura */
    gdt_set_entry(2,
        0x00000000,
        0xFFFFFFFF,
        GDT_PRESENT | GDT_RING0 | GDT_DESCRIPTOR | GDT_READWRITE,
        GDT_GRANULARITY | GDT_32BIT
    );

    /* Preparar el puntero y cargar la GDT */
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_flush((uint32_t)&gdt_ptr);
}
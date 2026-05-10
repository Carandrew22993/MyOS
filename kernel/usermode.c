/* usermode.c — Soporte para procesos en ring 3
 *
 * Para saltar a ring 3 necesitamos:
 * 1. Un stack en memoria de usuario (fuera del kernel)
 * 2. Actualizar el TSS con el stack del kernel para cuando lleguen IRQs
 * 3. Ejecutar iret con los selectores de ring 3
 *
 * Cuando el proceso usuario hace INT 0x80 (syscall):
 *   CPU ve que cambia de ring 3 → ring 0
 *   Lee esp0/ss0 del TSS → cambia al stack del kernel
 *   Ejecuta el handler de la IDT (syscall_handler)
 *   Al terminar, iret regresa a ring 3
 */

#include "usermode.h"
#include "pmm.h"
#include <stdint.h>

/* Stack de usuario en la región accesible desde ring 3 (4MB-8MB)
 * El kernel vive en 0-4MB (supervisor), el usuario usa 4MB-8MB */
#define USER_STACK_VIRT  0x00700000
#define USER_STACK_SIZE  0x1000

/* Stack del kernel para este proceso — el TSS lo necesita */

uint32_t usermode_alloc_stack(void) {
    /* La región 0x700000-0x800000 ya está en el identity map con PAGE_USER.
     * No necesitamos llamar paging_map — solo devolver el tope del stack. */
    return USER_STACK_VIRT + USER_STACK_SIZE;
}
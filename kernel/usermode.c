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
#include "paging.h"
#include "tss.h"
#include <stdint.h>

/* Stack de usuario: una página de 4KB en memoria virtual de usuario
 * Lo colocamos en 0x00C00000 (12MB) — por encima del heap del kernel */
#define USER_STACK_VIRT  0x00C00000
#define USER_STACK_SIZE  0x1000      /* 4KB */

/* Stack del kernel para este proceso — el TSS lo necesita */
static uint8_t kernel_stack[4096] __attribute__((aligned(16)));

uint32_t usermode_alloc_stack(void) {
    /* Pedir una página física */
    void* phys = pmm_alloc();
    if (!phys) return 0;

    /* Mapearla en el espacio virtual de usuario */
    paging_map(USER_STACK_VIRT, (uint32_t)phys,
               PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    /* Retornar el tope del stack (crece hacia abajo) */
    return USER_STACK_VIRT + USER_STACK_SIZE;
}
/* paging.c — Activar memoria virtual (paginación)
 *
 * Estrategia inicial: identity mapping
 * Mapeamos los primeros 8MB de memoria virtual → misma dirección física.
 * Eso significa que virt 0x100000 == phys 0x100000, etc.
 * El kernel sigue funcionando igual porque sus punteros no cambian.
 *
 * Cuando tengamos procesos de usuario, les daremos mapeos distintos
 * y cada proceso verá su propio espacio de direcciones.
 */

#include "paging.h"
#include "pmm.h"
#include <stdint.h>

/* El Page Directory tiene 1024 entradas, cada una cubre 4MB */
static pde_t page_directory[1024] __attribute__((aligned(4096)));

/* Page tables para los primeros 8MB (2 tablas × 4MB = 8MB) */
static pte_t page_tables[2][1024] __attribute__((aligned(4096)));

/* ── Escribir en CR3 y activar paginación en CR0 ──────────────────────── */
static void paging_enable(uint32_t page_dir_phys) {
    /* CR3 = dirección física del Page Directory */
    __asm__ volatile (
        "mov %0, %%cr3"
        : : "r"(page_dir_phys)
    );

    /* CR0: activar bit 31 (PG = Paging Enable) */
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1u << 31);
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}

/* ── Mapear una página virtual → física ───────────────────────────────── */
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx = VADDR_DIR(virt);
    uint32_t tbl_idx = VADDR_TABLE(virt);

    /* Si no hay page table para este directorio, crear una nueva */
    if (!(page_directory[dir_idx] & PAGE_PRESENT)) {
        void* new_table = pmm_alloc();
        if (!new_table) return; /* sin memoria */

        /* Limpiar la nueva tabla */
        pte_t* tbl = (pte_t*)new_table;
        for (int i = 0; i < 1024; i++) tbl[i] = 0;

        page_directory[dir_idx] = (uint32_t)new_table | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* Obtener puntero a la page table */
    pte_t* table = (pte_t*)(page_directory[dir_idx] & ~0xFFF);

    /* Escribir la entrada: dirección física + flags */
    table[tbl_idx] = (phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
}

/* ── Inicialización ────────────────────────────────────────────────────── */
void paging_init(void) {
    /* Limpiar el page directory */
    for (int i = 0; i < 1024; i++)
        page_directory[i] = 0;

    /* Limpiar las dos page tables iniciales */
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 1024; i++)
            page_tables[t][i] = 0;

    /* Identity map: primeros 8MB (2 page tables × 1024 páginas × 4KB) */
    for (int t = 0; t < 2; t++) {
        for (int i = 0; i < 1024; i++) {
            uint32_t phys = (uint32_t)(t * 1024 + i) * 0x1000;
            page_tables[t][i] = phys | PAGE_PRESENT | PAGE_WRITABLE;
        }
        /* Registrar la page table en el page directory */
        page_directory[t] = (uint32_t)page_tables[t] | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* Activar paginación */
    paging_enable((uint32_t)page_directory);
}
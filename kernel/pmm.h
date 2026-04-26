#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

/* El PMM divide toda la RAM en bloques de 4KB (páginas).
 * Usa un bitmap para saber cuáles están libres (0) y cuáles ocupadas (1).
 * Un bit por página → 1MB de bitmap cubre 32GB de RAM. */

#define PMM_PAGE_SIZE   4096          /* 4 KB por página */
#define PMM_PAGES_MAX   (1024 * 1024) /* máximo 1M páginas = 4GB de RAM */

void     pmm_init(uint32_t mem_size_kb, uint32_t kernel_start, uint32_t kernel_end);
void*    pmm_alloc(void);             /* reservar una página física → retorna dirección */
void     pmm_free(void* addr);        /* liberar una página física */
uint32_t pmm_free_pages(void);        /* cuántas páginas libres quedan */
uint32_t pmm_total_pages(void);       /* total de páginas disponibles */

#endif
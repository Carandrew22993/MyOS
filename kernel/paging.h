#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

/* x86 usa dos niveles de tablas para traducir direcciones virtuales a físicas:
 *
 *  Dirección virtual de 32 bits:
 *  ┌──────────┬──────────┬────────────────┐
 *  │ DIR (10) │ TBL (10) │  OFFSET (12)   │
 *  └──────────┴──────────┴────────────────┘
 *      │            │            │
 *      │            │            └─ byte dentro de la página (4KB)
 *      │            └─ índice en la Page Table (1024 entradas)
 *      └─ índice en el Page Directory (1024 entradas)
 *
 * Cada entrada (PDE / PTE) es un uint32_t con flags en los bits bajos
 * y la dirección física de la siguiente tabla en los bits altos.
 */

#define PAGE_PRESENT   (1 << 0)   /* la página está en memoria */
#define PAGE_WRITABLE  (1 << 1)   /* se puede escribir */
#define PAGE_USER      (1 << 2)   /* accesible desde ring 3 (usuario) */

/* Extraer índices de una dirección virtual */
#define VADDR_DIR(v)    (((uint32_t)(v)) >> 22)
#define VADDR_TABLE(v)  ((((uint32_t)(v)) >> 12) & 0x3FF)
#define VADDR_OFFSET(v) (((uint32_t)(v)) & 0xFFF)

/* Alinear una dirección hacia arriba al próximo múltiplo de 4096 */
#define PAGE_ALIGN(a)   (((uint32_t)(a) + 0xFFF) & ~0xFFF)

typedef uint32_t pde_t;   /* Page Directory Entry */
typedef uint32_t pte_t;   /* Page Table Entry     */

void paging_init(void);
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags);

#endif
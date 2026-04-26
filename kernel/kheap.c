/* kheap.c — Heap del kernel (kmalloc / kfree)
 *
 * Estrategia: lista enlazada de bloques (first-fit allocator)
 *
 * El heap vive en una región de memoria virtual fija.
 * Cada bloque tiene un header que describe su tamaño y si está libre.
 * Al hacer kmalloc buscamos el primer bloque libre suficientemente grande.
 * Al hacer kfree marcamos el bloque como libre y fusionamos bloques
 * adyacentes para evitar fragmentación.
 *
 * Layout de memoria:
 *
 *  HEAP_START
 *  ┌─────────────┬──────────────────────┐
 *  │   header    │       datos          │  bloque 1
 *  ├─────────────┼──────────────────────┤
 *  │   header    │       datos          │  bloque 2
 *  ├─────────────┼──────────────────────┤
 *  │     ...     │        ...           │
 *  └─────────────┴──────────────────────┘
 *  HEAP_START + HEAP_SIZE
 */

#include "kheap.h"
#include "pmm.h"
#include "paging.h"
#include <stdint.h>

/* El heap del kernel empieza en 8MB virtual (justo después del identity map)
 * y tiene 4MB de tamaño inicial — suficiente para estructuras de kernel */
#define HEAP_START  0x00800000   /* 8MB  */
#define HEAP_SIZE   0x00400000   /* 4MB  */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

/* Alineación mínima de cada bloque (8 bytes — seguro para cualquier tipo) */
#define ALIGN       8
#define ALIGN_UP(n) (((n) + ALIGN - 1) & ~(ALIGN - 1))

/* Header de cada bloque */
typedef struct block_header {
    size_t               size;    /* tamaño de los datos (sin contar el header) */
    uint32_t             free;    /* 1 = libre, 0 = ocupado */
    struct block_header* next;    /* siguiente bloque en la lista */
    struct block_header* prev;    /* bloque anterior */
    uint32_t             magic;   /* 0xCAFEBABE — detectar corrupción */
} block_header_t;

#define HEADER_SIZE   sizeof(block_header_t)
#define HEAP_MAGIC    0xCAFEBABE

static block_header_t* heap_start = (block_header_t*)HEAP_START;
static size_t bytes_used = 0;

/* ── Mapear páginas físicas para el heap ──────────────────────────────── */
static void heap_map_pages(void) {
    for (uint32_t virt = HEAP_START; virt < HEAP_END; virt += 0x1000) {
        void* phys = pmm_alloc();
        if (!phys) return; /* sin memoria física */
        paging_map(virt, (uint32_t)phys, PAGE_PRESENT | PAGE_WRITABLE);
    }
}

/* ── Inicializar el heap ──────────────────────────────────────────────── */
void kheap_init(void) {
    /* Mapear la región del heap en memoria virtual */
    heap_map_pages();

    /* Crear un único bloque libre que ocupa todo el heap */
    heap_start->size  = HEAP_SIZE - HEADER_SIZE;
    heap_start->free  = 1;
    heap_start->next  = (void*)0;
    heap_start->prev  = (void*)0;
    heap_start->magic = HEAP_MAGIC;
    bytes_used = 0;
}

/* ── Fusionar bloques libres adyacentes ───────────────────────────────── */
static void coalesce(block_header_t* block) {
    /* Fusionar con el siguiente si también está libre */
    if (block->next && block->next->free) {
        block->size += HEADER_SIZE + block->next->size;
        block->next  = block->next->next;
        if (block->next) block->next->prev = block;
    }
    /* Fusionar con el anterior si también está libre */
    if (block->prev && block->prev->free) {
        block->prev->size += HEADER_SIZE + block->size;
        block->prev->next  = block->next;
        if (block->next) block->next->prev = block->prev;
    }
}

/* ── Reservar memoria ─────────────────────────────────────────────────── */
void* kmalloc(size_t size) {
    if (size == 0) return (void*)0;

    size = ALIGN_UP(size); /* alinear a 8 bytes */

    /* Buscar el primer bloque libre suficientemente grande (first-fit) */
    block_header_t* current = heap_start;
    while (current) {
        if (current->magic != HEAP_MAGIC) return (void*)0; /* heap corrompido */

        if (current->free && current->size >= size) {
            /* ¿Tiene espacio para dividir en dos bloques? */
            if (current->size >= size + HEADER_SIZE + ALIGN) {
                /* Crear un nuevo bloque libre con el espacio sobrante */
                block_header_t* new_block = (block_header_t*)
                    ((uint8_t*)current + HEADER_SIZE + size);

                new_block->size  = current->size - size - HEADER_SIZE;
                new_block->free  = 1;
                new_block->next  = current->next;
                new_block->prev  = current;
                new_block->magic = HEAP_MAGIC;

                if (current->next) current->next->prev = new_block;
                current->next = new_block;
                current->size = size;
            }

            current->free = 0;
            bytes_used += current->size + HEADER_SIZE;
            return (uint8_t*)current + HEADER_SIZE;
        }
        current = current->next;
    }
    return (void*)0; /* sin memoria */
}

/* ── Liberar memoria ──────────────────────────────────────────────────── */
void kfree(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);

    if (block->magic != HEAP_MAGIC) return; /* puntero inválido */
    if (block->free) return;                /* doble free */

    bytes_used -= block->size + HEADER_SIZE;
    block->free = 1;
    coalesce(block);
}

/* ── kmalloc inicializado a cero ──────────────────────────────────────── */
void* kmalloc_zero(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr) return (void*)0;
    uint8_t* p = (uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) p[i] = 0;
    return ptr;
}

/* ── Estadísticas ─────────────────────────────────────────────────────── */
size_t kheap_used(void) { return bytes_used; }
size_t kheap_free(void) { return HEAP_SIZE - bytes_used; }
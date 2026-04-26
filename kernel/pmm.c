/* pmm.c — Physical Memory Manager
 *
 * Estrategia: bitmap de bits donde cada bit representa una página de 4KB.
 *   bit = 0 → página libre
 *   bit = 1 → página ocupada
 *
 * Al arrancar marcamos todo como ocupado y luego liberamos
 * solo las regiones que Multiboot2 nos dice que son RAM usable.
 * Después volvemos a marcar como ocupado el rango del kernel
 * para no pisarlo accidentalmente.
 */

#include "pmm.h"
#include <stdint.h>

/* ── Bitmap ────────────────────────────────────────────────────────────────
 * Guardamos el bitmap en una región fija de memoria justo después del kernel.
 * 1 bit por página → para 128MB necesitamos 128MB/4KB/8 = 4096 bytes = 4KB.
 * Para 4GB necesitamos 128KB de bitmap. */
static uint32_t  bitmap[PMM_PAGES_MAX / 32]; /* 32 bits por elemento */
static uint32_t  total_pages = 0;
static uint32_t  free_pages  = 0;

/* ── Operaciones sobre el bitmap ───────────────────────────────────────── */
static void bitmap_set(uint32_t page) {
    bitmap[page / 32] |= (1u << (page % 32));
}

static void bitmap_clear(uint32_t page) {
    bitmap[page / 32] &= ~(1u << (page % 32));
}

static int bitmap_test(uint32_t page) {
    return bitmap[page / 32] & (1u << (page % 32));
}

/* ── Marcar rangos de memoria ──────────────────────────────────────────── */
static void pmm_mark_used(uint32_t start_addr, uint32_t size) {
    uint32_t page_start = start_addr / PMM_PAGE_SIZE;
    uint32_t page_count = (size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (uint32_t i = 0; i < page_count; i++) {
        if (!bitmap_test(page_start + i)) {
            bitmap_set(page_start + i);
            if (free_pages > 0) free_pages--;
        }
    }
}

static void pmm_mark_free(uint32_t start_addr, uint32_t size) {
    uint32_t page_start = start_addr / PMM_PAGE_SIZE;
    uint32_t page_count = (size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    for (uint32_t i = 0; i < page_count; i++) {
        uint32_t page = page_start + i;
        if (page < total_pages && bitmap_test(page)) {
            bitmap_clear(page);
            free_pages++;
        }
    }
}

/* ── Inicialización ────────────────────────────────────────────────────── */
/* mem_size_kb   : RAM total en kilobytes (viene de Multiboot2)
 * kernel_start  : dirección física donde empieza el kernel
 * kernel_end    : dirección física donde termina el kernel  */
void pmm_init(uint32_t mem_size_kb, uint32_t kernel_start, uint32_t kernel_end) {
    uint32_t mem_bytes = mem_size_kb * 1024;
    total_pages = mem_bytes / PMM_PAGE_SIZE;
    free_pages  = 0;

    /* Empezar con TODO marcado como ocupado */
    for (uint32_t i = 0; i < PMM_PAGES_MAX / 32; i++) {
        bitmap[i] = 0xFFFFFFFF;
    }

    /* Liberar la RAM convencional (empieza en 1MB, termina en mem_size) */
    pmm_mark_free(0x100000, mem_bytes - 0x100000);

    /* Volver a marcar el kernel como ocupado para no pisarlo */
    pmm_mark_used(kernel_start, kernel_end - kernel_start);

    /* Marcar el bitmap mismo como ocupado */
    pmm_mark_used((uint32_t)bitmap, sizeof(bitmap));

    /* Marcar el primer MB como ocupado (BIOS, VGA, etc.) */
    pmm_mark_used(0, 0x100000);
}

/* ── Reservar una página ───────────────────────────────────────────────── */
/* Retorna la dirección física de la página, o NULL si no hay memoria */
void* pmm_alloc(void) {
    if (free_pages == 0) return (void*)0;

    /* Buscar el primer bit en 0 (página libre) */
    for (uint32_t i = 0; i < total_pages / 32; i++) {
        if (bitmap[i] != 0xFFFFFFFF) {          /* hay al menos un bit libre en este grupo */
            for (uint32_t j = 0; j < 32; j++) {
                uint32_t page = i * 32 + j;
                if (!bitmap_test(page)) {
                    bitmap_set(page);
                    free_pages--;
                    return (void*)(page * PMM_PAGE_SIZE);
                }
            }
        }
    }
    return (void*)0; /* no hay páginas libres */
}

/* ── Liberar una página ────────────────────────────────────────────────── */
void pmm_free(void* addr) {
    uint32_t page = (uint32_t)addr / PMM_PAGE_SIZE;
    if (page < total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

/* ── Consultas ─────────────────────────────────────────────────────────── */
uint32_t pmm_free_pages(void)  { return free_pages;  }
uint32_t pmm_total_pages(void) { return total_pages; }
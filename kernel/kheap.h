#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>

void  kheap_init(void);
void* kmalloc(size_t size);
void  kfree(void* ptr);
void* kmalloc_zero(size_t size);   /* kmalloc + inicializar a cero */

/* Estadísticas */
size_t kheap_used(void);
size_t kheap_free(void);

#endif
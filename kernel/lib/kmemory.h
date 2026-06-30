#ifndef KMEMORY_H
#define KMEMORY_H

#include <stddef.h>

/* kmemory.h — Librería base de manejo de memoria
 *
 * Misma regla que kstring.h: sin dependencias de heap ni de otros
 * subsistemas. Opera siempre sobre buffers ya reservados por el caller.
 */

/* Copia n bytes de src a dst. Los bloques NO deben superponerse
 * (no es memmove). Retorna dst. */
void* kmemcpy(void* dst, const void* src, size_t n);

/* Llena n bytes de dst con el byte `val`. Retorna dst. */
void* kmemset(void* dst, int val, size_t n);

/* Compara n bytes de dos bloques de memoria.
 * Retorna 0 si son iguales, !=0 en otro caso (mismo contrato que memcmp). */
int kmemcmp(const void* a, const void* b, size_t n);

#endif

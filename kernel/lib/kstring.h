#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>

/* kstring.h — Librería base de manejo de strings
 *
 * Regla de diseño: este módulo NO depende de ningún otro subsistema
 * del kernel (ni heap, ni terminal, ni VFS). Solo usa <stddef.h>.
 * Esto permite reutilizarlo en cualquier contexto: bootloader,
 * manejo de excepciones, o incluso pruebas aisladas.
 *
 * No reserva memoria. Si una función necesita un buffer, lo recibe
 * como parámetro junto con su tamaño máximo.
 */

/* Longitud de un string, sin contar el terminador nulo. */
size_t kstrlen(const char* s);

/* Comparación completa: 0 si son iguales, !=0 en otro caso
 * (mismo contrato que strcmp). */
int kstrcmp(const char* a, const char* b);

/* Comparación de los primeros n caracteres. */
int kstrncmp(const char* a, const char* b, size_t n);

/* Copia src a dst, truncando si es necesario para no exceder `max`
 * (incluyendo el terminador nulo). Siempre garantiza dst[max-1] = '\0'.
 * Retorna dst. */
char* kstrcpy(char* dst, const char* src, size_t max);

#endif

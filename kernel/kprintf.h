#ifndef KPRINTF_H
#define KPRINTF_H

/* kprintf.h — Formateador de salida del kernel
 *
 * Especificadores soportados: %s %c %d %u %x %X %%
 * Intencionalmente limitado: sin ancho de campo ni padding.
 */

void kprintf(const char* fmt, ...);

#endif
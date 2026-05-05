#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

/* Salta al entry point dado en ring 3.
 * eip     = dirección donde empieza el proceso usuario
 * esp     = tope del stack del proceso usuario
 * No retorna — a partir de aquí el proceso corre en ring 3. */
void jump_to_usermode(uint32_t eip, uint32_t esp);

/* Asignar una región de memoria para el stack de usuario */
uint32_t usermode_alloc_stack(void);

#endif
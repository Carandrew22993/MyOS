#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Las syscalls son la única puerta de entrada del ring 3 al kernel.
 * Un proceso de usuario ejecuta INT 0x80 con:
 *   EAX = número de syscall
 *   EBX, ECX, EDX = argumentos
 *
 * El kernel atiende la interrupción, ejecuta la función,
 * y retorna el resultado en EAX.
 *
 * Syscalls disponibles (inspiradas en Linux): */
#define SYS_EXIT    0   /* terminar el proceso */
#define SYS_WRITE   1   /* escribir en pantalla */
#define SYS_GETPID  2   /* obtener el PID actual */
#define SYS_SLEEP   3   /* dormir N milisegundos */
#define SYS_YIELD   4   /* ceder el CPU */

void syscall_init(void);

#endif
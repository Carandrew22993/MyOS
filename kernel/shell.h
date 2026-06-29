#ifndef SHELL_H
#define SHELL_H

/* shell.h — Interfaz pública del shell interactivo
 *
 * El shell es el único módulo con acceso al estado de la sesión
 * (directorio actual, buffer de línea). Ningún otro módulo
 * necesita conocer esos detalles.
 */

void shell_init(void);   /* inicializa estado interno */
void shell_run(void);    /* bucle principal — no retorna */

#endif

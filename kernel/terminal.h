#ifndef TERMINAL_H
#define TERMINAL_H

/* terminal.h — Interfaz pública del driver de terminal
 *
 * Este módulo abstrae la salida por pantalla del kernel.
 * El resto del kernel no conoce nada sobre VGA, buffers de video
 * ni direcciones de memoria de hardware — solo habla con esta interfaz.
 *
 * Si en el futuro cambiamos el backend (framebuffer, puerto serie, etc.)
 * ningún módulo que incluya este header necesita modificarse.
 */

/* Colores disponibles — independientes de la implementación VGA.
 * Los valores coinciden con los índices de color VGA por conveniencia,
 * pero eso es un detalle de implementación de terminal.c. */
typedef enum {
    COLOR_BLACK   = 0,
    COLOR_BLUE    = 1,
    COLOR_GREEN   = 2,
    COLOR_CYAN    = 3,
    COLOR_RED     = 4,
    COLOR_MAGENTA = 5,
    COLOR_BROWN   = 6,
    COLOR_LGRAY   = 7,
    COLOR_DGRAY   = 8,
    COLOR_LBLUE   = 9,
    COLOR_LGREEN  = 10,
    COLOR_LCYAN   = 11,
    COLOR_LRED    = 12,
    COLOR_PINK    = 13,
    COLOR_YELLOW  = 14,
    COLOR_WHITE   = 15,
} terminal_color_t;

void terminal_init(void);
void terminal_putchar(char c);
void terminal_print(const char* str);
void terminal_set_color(terminal_color_t fg, terminal_color_t bg);
void terminal_reset_color(void);   /* vuelve a COLOR_WHITE sobre COLOR_BLACK */

#endif

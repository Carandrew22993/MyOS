/* terminal.c — Driver de terminal sobre VGA text mode
 *
 * Implementación concreta de terminal.h usando el buffer de texto VGA
 * en 0xB8000. Este es el único archivo del kernel que conoce detalles
 * de VGA: dimensiones, formato de celda, dirección del buffer.
 *
 * Cualquier otro módulo que necesite escribir en pantalla incluye
 * terminal.h — nunca este archivo directamente.
 */

#include "terminal.h"
#include <stdint.h>
#include <stddef.h>

/* ── Constantes VGA ────────────────────────────────────────────────────── */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((volatile uint16_t*)0xB8000)

#define DEFAULT_FG  COLOR_WHITE
#define DEFAULT_BG  COLOR_BLACK

/* ── Estado interno ────────────────────────────────────────────────────── */
static size_t  terminal_row;
static size_t  terminal_col;
static uint8_t terminal_color;

/* ── Helpers privados ──────────────────────────────────────────────────── */
static uint8_t make_color(terminal_color_t fg, terminal_color_t bg) {
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

static uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

static void scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];

    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            make_entry(' ', terminal_color);

    terminal_row = VGA_HEIGHT - 1;
}

/* ── API pública ───────────────────────────────────────────────────────── */
void terminal_init(void) {
    terminal_row   = 0;
    terminal_col   = 0;
    terminal_color = make_color(DEFAULT_FG, DEFAULT_BG);

    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = make_entry(' ', terminal_color);
}

void terminal_set_color(terminal_color_t fg, terminal_color_t bg) {
    terminal_color = make_color(fg, bg);
}

void terminal_reset_color(void) {
    terminal_color = make_color(DEFAULT_FG, DEFAULT_BG);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_col = 0;
        if (++terminal_row == VGA_HEIGHT) scroll();
        return;
    }

    if (c == '\b') {
        if (terminal_col > 0) {
            --terminal_col;
        } else if (terminal_row > 0) {
            --terminal_row;
            terminal_col = VGA_WIDTH - 1;
        }
        VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_col] =
            make_entry(' ', terminal_color);
        return;
    }

    VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_col] =
        make_entry(c, terminal_color);

    if (++terminal_col == VGA_WIDTH) {
        terminal_col = 0;
        if (++terminal_row == VGA_HEIGHT) scroll();
    }
}

void terminal_print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++)
        terminal_putchar(str[i]);
}

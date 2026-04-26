/* keyboard.c — Driver de teclado PS/2
 *
 * El teclado genera una interrupción (IRQ1) por cada tecla presionada/soltada.
 * Leemos el scancode del puerto 0x60 y lo traducimos a ASCII.
 *
 * Usamos el mapa de scancodes Set 1 (el estándar en x86).
 * Scancodes < 0x80 = tecla presionada, >= 0x80 = tecla soltada (make | 0x80).
 */

#include "keyboard.h"
#include "idt.h"
#include <stdint.h>

#define KEYBOARD_DATA_PORT 0x60

/* Buffer circular para teclas */
#define KB_BUFFER_SIZE 256
static char     kb_buffer[KB_BUFFER_SIZE];
static uint32_t kb_read  = 0;
static uint32_t kb_write = 0;

/* Estado de teclas modificadoras */
static uint8_t shift_pressed = 0;
static uint8_t caps_lock     = 0;

/* Tabla de scancodes Set 1 → ASCII (sin shift) */
static const char scancode_map[] = {
/*00*/ 0,    0,   '1', '2', '3', '4', '5', '6',
/*08*/ '7', '8', '9', '0', '-', '=',  '\b', '\t',
/*10*/ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
/*18*/ 'o', 'p', '[', ']', '\n', 0,  'a', 's',
/*20*/ 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
/*28*/ '\'','`',  0,  '\\','z', 'x', 'c', 'v',
/*30*/ 'b', 'n', 'm', ',', '.', '/', 0,   '*',
/*38*/ 0,   ' ', 0,   0,   0,   0,   0,   0,
/*40*/ 0,   0,   0,   0,   0,   0,   0,   '7',
/*48*/ '8', '9', '-', '4', '5', '6', '+', '1',
/*50*/ '2', '3', '0', '.'
};

/* Tabla con shift presionado */
static const char scancode_map_shift[] = {
/*00*/ 0,    0,   '!', '@', '#', '$', '%', '^',
/*08*/ '&', '*', '(', ')', '_', '+',  '\b', '\t',
/*10*/ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
/*18*/ 'O', 'P', '{', '}', '\n', 0,  'A', 'S',
/*20*/ 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
/*28*/ '"', '~',  0,  '|', 'Z', 'X', 'C', 'V',
/*30*/ 'B', 'N', 'M', '<', '>', '?', 0,   '*',
/*38*/ 0,   ' '
};

#define SCANCODE_MAP_SIZE ((uint8_t)(sizeof(scancode_map) / sizeof(scancode_map[0])))
#define SHIFT_MAP_SIZE    ((uint8_t)(sizeof(scancode_map_shift) / sizeof(scancode_map_shift[0])))

/* Scancodes especiales */
#define SC_LSHIFT_PRESS   0x2A
#define SC_RSHIFT_PRESS   0x36
#define SC_LSHIFT_RELEASE 0xAA
#define SC_RSHIFT_RELEASE 0xB6
#define SC_CAPS_LOCK      0x3A

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void keyboard_callback(interrupt_frame_t* frame) {
    (void)frame;
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* Manejar modificadores */
    if (scancode == SC_LSHIFT_PRESS || scancode == SC_RSHIFT_PRESS) {
        shift_pressed = 1; return;
    }
    if (scancode == SC_LSHIFT_RELEASE || scancode == SC_RSHIFT_RELEASE) {
        shift_pressed = 0; return;
    }
    if (scancode == SC_CAPS_LOCK) {
        caps_lock = !caps_lock; return;
    }

    /* Ignorar key-release events (bit 7 activo) */
    if (scancode & 0x80) return;

    /* Traducir scancode a ASCII */
    char c = 0;
    if (shift_pressed && scancode < SHIFT_MAP_SIZE) {
        c = scancode_map_shift[scancode];
    } else if (!shift_pressed && scancode < SCANCODE_MAP_SIZE) {
        c = scancode_map[scancode];
        /* Aplicar caps lock solo a letras */
        if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
    }

    /* Guardar en buffer circular si hay espacio */
    if (c != 0) {
        uint32_t next = (kb_write + 1) % KB_BUFFER_SIZE;
        if (next != kb_read) {
            kb_buffer[kb_write] = c;
            kb_write = next;
        }
    }
}

void keyboard_init(void) {
    irq_register(1, keyboard_callback);
}

/* Retorna 1 si hay una tecla disponible en el buffer */
int keyboard_haskey(void) {
    return kb_read != kb_write;
}

/* Retorna el siguiente carácter del buffer (bloquea hasta que haya uno) */
char keyboard_getchar(void) {
    while (!keyboard_haskey()) {
        __asm__ volatile ("hlt");
    }
    char c = kb_buffer[kb_read];
    kb_read = (kb_read + 1) % KB_BUFFER_SIZE;
    return c;
}
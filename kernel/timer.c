/* timer.c — PIT (Programmable Interval Timer)
 *
 * El PIT genera una interrupción (IRQ0) a una frecuencia configurable.
 * Lo usamos para tener un "tick" del sistema — base de todo scheduling futuro.
 *
 * Frecuencia base del PIT: 1,193,182 Hz
 * divisor = 1193182 / frecuencia_deseada
 */

#include "timer.h"
#include "idt.h"
#include "scheduler.h"
#include <stdint.h>


#define PIT_CHANNEL0  0x40   /* canal 0 — conectado a IRQ0 */
#define PIT_CMD       0x43   /* registro de comando */
#define PIT_FREQUENCY 1193182

static volatile uint32_t ticks = 0;
static uint32_t timer_hz = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void timer_callback(interrupt_frame_t* frame) {
    (void)frame;
    ticks++;
    scheduler_tick();
}

void timer_init(uint32_t frequency) {
    timer_hz = frequency;

    /* Calcular divisor */
    uint32_t divisor = PIT_FREQUENCY / frequency;

    /* Comando: canal 0, acceso lo/hi, modo 3 (square wave), binario */
    outb(PIT_CMD, 0x36);

    /* Enviar divisor en dos bytes (low byte primero) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Registrar el handler para IRQ0 */
    irq_register(0, timer_callback);
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

/* Esperar aproximadamente ms milisegundos */
void timer_sleep(uint32_t ms) {
    uint32_t target = ticks + (timer_hz * ms / 1000);
    while (ticks < target) {
        __asm__ volatile ("hlt");
    }
}
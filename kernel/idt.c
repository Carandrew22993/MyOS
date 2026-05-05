/* idt.c — Interrupt Descriptor Table
 *
 * El CPU x86 tiene 256 entradas en la IDT:
 *   0-31  → Excepciones del CPU (division por cero, page fault, etc.)
 *   32-47 → IRQs de hardware (timer, teclado, disco, etc.) remapeadas por el PIC
 *   48+   → Interrupciones de software (syscalls, etc.)
 *
 * El PIC (Programmable Interrupt Controller) mapea las IRQs de hardware
 * a los vectores 0-15 por defecto, pero eso colisiona con las excepciones
 * del CPU. Lo remapeamos a los vectores 32-47.
 */

#include "idt.h"
#include <stdint.h>

/* Puertos del PIC maestro y esclavo */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20    /* End Of Interrupt */

#define IDT_ENTRIES 256

/* Bits del byte type_attr */
#define IDT_PRESENT    (1 << 7)
#define IDT_RING0      (0 << 5)
#define IDT_GATE_INT32 0x0E    /* interrupt gate de 32 bits (deshabilita IRQs al entrar) */

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

/* Handler genérico en C — recibe el frame construido por los stubs ASM */
typedef void (*isr_handler_t)(interrupt_frame_t*);
static isr_handler_t handlers[IDT_ENTRIES] = {0};

/* Stubs ASM declarados en idt_asm.asm */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

extern void idt_flush(uint32_t);

/* Nombres de las excepciones para mostrar en pantalla */
static const char* exception_names[] = {
    "Division por cero",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Opcode invalido",
    "FPU no disponible",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "TSS invalido",
    "Segmento no presente",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reservado",
    "Error FPU x87",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
};

/* ── Funciones de I/O ──────────────────────────────────────────────────── */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0); /* escribir al puerto 0x80 introduce un pequeño delay */
}

/* ── Configurar una entrada de la IDT ─────────────────────────────────── */
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

/* ── Remap del PIC ─────────────────────────────────────────────────────── */
static void pic_remap(void) {
    /* Guardar máscaras actuales */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Inicialización en cascada (ICW1) */
    outb(PIC1_CMD,  0x11); io_wait();
    outb(PIC2_CMD,  0x11); io_wait();

    /* Vectores base (ICW2): IRQ0-7 → INT 32-39, IRQ8-15 → INT 40-47 */
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();

    /* Cascada (ICW3) */
    outb(PIC1_DATA, 0x04); io_wait(); /* PIC1: esclavo en IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait(); /* PIC2: identidad de cascada */

    /* Modo 8086 (ICW4) */
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    /* Restaurar máscaras */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* ── Inicializar la IDT ─────────────────────────────────────────────────── */
void idt_init(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base  = (uint32_t)&idt;

    /* Limpiar toda la IDT */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Remap del PIC antes de instalar IRQs */
    pic_remap();

    /* Instalar excepciones del CPU (ISR 0-31) */
    idt_set_gate(0,  (uint32_t)isr0,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_PRESENT | IDT_GATE_INT32);

    /* Instalar IRQs de hardware (32-47) */
    idt_set_gate(32, (uint32_t)irq0,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(33, (uint32_t)irq1,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(34, (uint32_t)irq2,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(35, (uint32_t)irq3,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(36, (uint32_t)irq4,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(37, (uint32_t)irq5,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(38, (uint32_t)irq6,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(39, (uint32_t)irq7,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(40, (uint32_t)irq8,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(41, (uint32_t)irq9,  0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(42, (uint32_t)irq10, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(43, (uint32_t)irq11, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(44, (uint32_t)irq12, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(45, (uint32_t)irq13, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(46, (uint32_t)irq14, 0x08, IDT_PRESENT | IDT_GATE_INT32);
    idt_set_gate(47, (uint32_t)irq15, 0x08, IDT_PRESENT | IDT_GATE_INT32);

    idt_flush((uint32_t)&idt_ptr);
}

/* ── Dispatcher central ─────────────────────────────────────────────────── */

/* Registrar un handler para una IRQ de hardware (0-15) */
void irq_register(uint8_t irq, isr_handler_t handler) {
    handlers[32 + irq] = handler;
}

/* Llamado desde los stubs ASM para excepciones del CPU */
void isr_handler(interrupt_frame_t* frame) {
    if (frame->int_no < 20) {
        /* Excepción del CPU — mostrar en pantalla y colgar */
        /* En el futuro aquí iría el kernel panic con stack trace */
        (void)exception_names[frame->int_no]; /* usado cuando haya terminal_print */
        __asm__ volatile ("cli; hlt");
    }
}

/* Llamado desde los stubs ASM para IRQs de hardware */
void irq_handler(interrupt_frame_t* frame) {
    /* Enviar EOI al PIC esclavo si la IRQ viene de él (IRQ 8-15) */
    if (frame->int_no >= 40) {
        outb(PIC2_CMD, PIC_EOI);
    }
    /* Siempre enviar EOI al PIC maestro */
    outb(PIC1_CMD, PIC_EOI);

    /* Llamar al handler registrado si existe */
    if (handlers[frame->int_no]) {
        handlers[frame->int_no](frame);
    }
}

/* Registrar una entrada en la IDT accesible desde ring 3 (DPL=3)
 * Usado por syscall_init para INT 0x80 */
void idt_set_gate_user(uint8_t num, uint32_t base) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector    = 0x08;
    idt[num].zero        = 0;
    /* IDT_PRESENT | DPL=3 | interrupt gate 32-bit */
    idt[num].type_attr   = (1 << 7) | (3 << 5) | 0x0E;
}
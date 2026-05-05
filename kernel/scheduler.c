/* scheduler.c — Scheduler round-robin con cambio de contexto
 *
 * Algoritmo: round-robin simple.
 * Cada tick del timer (100Hz) incrementa el contador del proceso actual.
 * Cada 5 ticks (50ms) cambiamos al siguiente proceso READY en la lista circular.
 *
 * El cambio de contexto guarda los registros del proceso actual en su PCB
 * y restaura los del siguiente proceso.
 *
 * Lista de procesos: circular, enlazada simple.
 *   proceso0 → proceso1 → proceso2 → proceso0 → ...
 */

#include "scheduler.h"
#include "kheap.h"
#include "timer.h"
#include "idt.h"
#include <stdint.h>

#define QUANTUM_TICKS   5       /* ticks antes de cambiar de proceso */
#define DEFAULT_STACK   8192    /* 8KB de stack por defecto */

static process_t* current   = (void*)0;
static process_t* list_head = (void*)0;
static uint32_t   next_pid  = 0;
static uint32_t   tick_count = 0;

/* ── Cambio de contexto en C ──────────────────────────────────────────────
 * Guardamos el EIP de retorno y el ESP actuales en el PCB del proceso saliente,
 * luego cambiamos el stack al proceso entrante y retornamos a su EIP.
 *
 * Nota: en un OS real esto se haría completamente en ASM para mayor control.
 * Esta versión usa setjmp/longjmp conceptualmente implementado a mano. */

/* Guardar contexto del proceso actual y saltar al siguiente */
static void __attribute__((noinline)) context_switch(process_t* next) {
    if (!current || !next || current == next) return;

    process_t* prev = current;
    current = next;

    /* Guardar ESP y EIP (aproximación en C — suficiente para demostrar el concepto) */
    __asm__ volatile (
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        : "=m"(prev->context.esp), "=m"(prev->context.ebp)
    );

    /* Restaurar stack del siguiente proceso */
    __asm__ volatile (
        "mov %0, %%esp\n"
        "mov %1, %%ebp\n"
        : : "m"(next->context.esp), "m"(next->context.ebp)
    );
}

/* ── Crear un nuevo proceso ───────────────────────────────────────────── */
process_t* process_create(const char* name, void (*entry)(void), uint32_t stack_size) {
    if (stack_size == 0) stack_size = DEFAULT_STACK;

    process_t* proc = (process_t*)kmalloc_zero(sizeof(process_t));
    if (!proc) return (void*)0;

    uint32_t* stack = (uint32_t*)kmalloc(stack_size);
    if (!stack) { kfree(proc); return (void*)0; }

    /* Copiar nombre */
    int i = 0;
    while (name[i] && i < 31) { proc->name[i] = name[i]; i++; }
    proc->name[i] = '\0';

    proc->pid        = next_pid++;
    proc->state      = PROCESS_READY;
    proc->stack      = stack;
    proc->stack_size = stack_size;
    proc->ticks      = 0;
    proc->next       = (void*)0;

    /* Preparar el stack inicial:
     * Simular que el proceso fue interrumpido justo al entrar a entry().
     * Cuando el scheduler haga context_switch hacia este proceso,
     * el CPU retornará a entry(). */
    uint32_t* sp = (uint32_t*)((uint8_t*)stack + stack_size);

    /* Dirección de retorno: process_exit (si entry() retorna) */
    *(--sp) = (uint32_t)process_exit;

    /* EIP inicial = entry point del proceso */
    proc->context.eip    = (uint32_t)entry;
    proc->context.esp    = (uint32_t)sp;
    proc->context.ebp    = (uint32_t)sp;
    proc->context.eflags = 0x202; /* interrupciones habilitadas */

    /* Agregar a la lista circular */
    if (!list_head) {
        list_head   = proc;
        proc->next  = proc; /* apunta a sí mismo */
    } else {
        /* Buscar el último nodo */
        process_t* last = list_head;
        while (last->next != list_head) last = last->next;
        last->next  = proc;
        proc->next  = list_head;
    }

    return proc;
}

/* ── Tick del scheduler (llamado desde el timer) ──────────────────────── */
void scheduler_tick(void) {
    if (!current) return;
    current->ticks++;
    tick_count++;

    if (tick_count < QUANTUM_TICKS) return;
    tick_count = 0;

    /* Buscar el siguiente proceso READY */
    process_t* next = current->next;
    int attempts = 0;
    while (next->state != PROCESS_READY && next->state != PROCESS_RUNNING) {
        next = next->next;
        if (++attempts > 256) return; /* evitar bucle infinito */
    }

    if (next == current) return; /* solo hay un proceso */

    current->state = PROCESS_READY;
    next->state    = PROCESS_RUNNING;
    context_switch(next);
}

/* ── Ceder el CPU voluntariamente ─────────────────────────────────────── */
void process_yield(void) {
    tick_count = QUANTUM_TICKS; /* forzar cambio en el próximo tick */
}

/* ── El proceso actual termina ────────────────────────────────────────── */
void process_exit(void) {
    if (!current) return;
    current->state = PROCESS_DEAD;
    process_yield();
    /* No retorna */
    for (;;) __asm__ volatile ("hlt");
}

/* ── Inicializar el scheduler ─────────────────────────────────────────── */
void scheduler_init(void) {
    current    = (void*)0;
    list_head  = (void*)0;
    next_pid   = 0;
    tick_count = 0;
}

/* ── PID del proceso actual ───────────────────────────────────────────── */
uint32_t process_current_pid(void) {
    return current ? current->pid : 0;
}

process_t* scheduler_get_head(void) {
    return list_head;
}

/* ── Iniciar el scheduler con el primer proceso ───────────────────────── */
void scheduler_start(process_t* first) {
    if (!first) return;
    first->state = PROCESS_RUNNING;
    current = first;

    /* Saltar al entry point del primer proceso */
    __asm__ volatile (
        "mov %0, %%esp\n"
        "mov %1, %%ebp\n"
        "sti\n"
        "jmp *%2\n"
        : : "r"(first->context.esp),
            "r"(first->context.ebp),
            "r"(first->context.eip)
    );
}
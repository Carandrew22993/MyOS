#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/* Estados posibles de un proceso */
typedef enum {
    PROCESS_READY,    /* listo para ejecutar */
    PROCESS_RUNNING,  /* ejecutándose ahora */
    PROCESS_BLOCKED,  /* esperando algo (I/O, sleep, etc.) */
    PROCESS_DEAD,     /* terminado, pendiente de limpieza */
} process_state_t;

/* Contexto del CPU — lo que guardamos al cambiar de proceso.
 * El orden importa: debe coincidir con el pushad en el cambio de contexto ASM. */
typedef struct {
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t eip;       /* instrucción donde reanudar */
    uint32_t eflags;    /* flags del CPU */
} cpu_context_t;

/* Bloque de control de proceso (PCB) */
typedef struct process {
    uint32_t         pid;           /* identificador único */
    char             name[32];      /* nombre del proceso */
    process_state_t  state;         /* estado actual */
    cpu_context_t    context;       /* contexto guardado del CPU */
    uint32_t*        stack;         /* base del stack del proceso */
    uint32_t         stack_size;    /* tamaño del stack en bytes */
    uint32_t         ticks;         /* ticks de CPU consumidos */
    struct process*  next;          /* siguiente en la lista circular */
} process_t;

void      scheduler_init(void);
process_t* process_create(const char* name, void (*entry)(void), uint32_t stack_size);
void      scheduler_tick(void);
void      process_exit(void);
uint32_t  process_current_pid(void);
void      process_yield(void);
void      scheduler_start(process_t* first);
process_t* scheduler_get_head(void);

#endif
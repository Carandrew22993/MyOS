/* kernel.c — Punto de entrada del kernel y orquestador de subsistemas
 *
 * Responsabilidad única: inicializar cada subsistema en el orden correcto
 * y transferir el control al primer proceso de usuario.
 *
 * Este archivo no contiene lógica de negocio. Si se encuentra aquí
 * una función que no sea de inicialización u orquestación, pertenece
 * a otro módulo.
 */

#include <stdint.h>
#include <stddef.h>

#include "terminal.h"
#include "kprintf.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "scheduler.h"
#include "syscall.h"
#include "vfs.h"
#include "usermode.h"
#include "elf.h"
#include "shell.h"

/* ── Parsing de Multiboot2 ─────────────────────────────────────────────── */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_info_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed)) mb2_tag_mmap_t;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

#define MB2_MAGIC       0x36d76289
#define MB2_TAG_MMAP    6
#define MB2_MEM_USABLE  1

static uint32_t mb2_detect_memory(uint32_t magic, void* mbi) {
    if (magic != MB2_MAGIC || !mbi) return 0;

    uint32_t mem_kb = 0;
    mb2_tag_t* tag = (mb2_tag_t*)((uint8_t*)mbi + 8);

    while (tag->type != 0) {
        if (tag->type == MB2_TAG_MMAP) {
            mb2_tag_mmap_t*   mt    = (mb2_tag_mmap_t*)tag;
            mb2_mmap_entry_t* entry = (mb2_mmap_entry_t*)((uint8_t*)mt + 16);
            uint32_t entries = (mt->size - 16) / mt->entry_size;

            for (uint32_t i = 0; i < entries; i++) {
                if (entry->type == MB2_MEM_USABLE) {
                    uint32_t top = (uint32_t)(entry->base_addr + entry->length);
                    if (top / 1024 > mem_kb) mem_kb = top / 1024;
                }
                entry = (mb2_mmap_entry_t*)((uint8_t*)entry + mt->entry_size);
            }
        }
        tag = (mb2_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7));
    }
    return mem_kb;
}

/* ── Símbolos del linker ───────────────────────────────────────────────── */
extern uint32_t _start_kernel;
extern uint32_t _end_kernel;

/* ── Punto de retorno tras SYS_EXIT del proceso init ──────────────────── */
extern uint8_t  hello_elf_data[];
extern uint32_t hello_elf_size;

void __attribute__((noreturn)) kernel_after_usermode(void) {
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        : : : "ax"
    );
    terminal_set_color(COLOR_LGREEN, COLOR_BLACK);
    terminal_print("[OK] Proceso init terminado, iniciando shell\n\n");
    terminal_reset_color();

    shell_run();
    while (1);
}

/* ── Punto de entrada principal ────────────────────────────────────────── */
void kernel_main(uint32_t magic, void* mbi) {

    /* Terminal primero — necesario para cualquier salida posterior */
    terminal_init();

    terminal_set_color(COLOR_LGREEN, COLOR_BLACK);
    terminal_print("myOS v0.1\n=========\n\n");
    terminal_reset_color();

    if (magic == MB2_MAGIC) {
        terminal_print("[OK] Cargado correctamente por GRUB (Multiboot2)\n");
    } else {
        terminal_set_color(COLOR_LRED, COLOR_BLACK);
        terminal_print("[WARN] Magic number incorrecto\n");
        terminal_reset_color();
    }

    terminal_print("[OK] Terminal inicializado\n");
    terminal_print("[OK] Kernel en modo protegido 32-bit\n\n");

    /* Subsistemas base de CPU */
    gdt_init();
    terminal_print("[OK] GDT configurada\n");

    idt_init();
    terminal_print("[OK] IDT configurada (32 excepciones + 16 IRQs)\n");
    terminal_print("[OK] GDT+TSS configurados (ring 0/3)\n");

    syscall_init();
    terminal_print("[OK] Syscalls listas (INT 0x80)\n");

    /* Sistema de archivos virtual */
    vfs_init();
    terminal_print("[OK] VFS inicializado (/, /bin, /etc, /home, /tmp)\n");

    extern int vfs_create_binary(const char* path, uint8_t* data, uint32_t size);
    vfs_create_binary("/bin/hello", hello_elf_data, hello_elf_size);

    /* Hardware */
    timer_init(100);
    terminal_print("[OK] Timer PIT inicializado (100 Hz)\n");

    keyboard_init();
    terminal_print("[OK] Teclado PS/2 listo\n");

    __asm__ volatile ("sti");
    terminal_print("[OK] Interrupciones habilitadas\n");

    /* Memoria física y virtual */
    uint32_t mem_kb = mb2_detect_memory(magic, mbi);
    if (mem_kb == 0) mem_kb = 32 * 1024;

    uint32_t kstart = (uint32_t)&_start_kernel;
    uint32_t kend   = (uint32_t)&_end_kernel;
    pmm_init(mem_kb, kstart, kend);

    paging_init();
    terminal_print("[OK] Paginacion activada (identity map 8MB)\n");

    kheap_init();
    terminal_print("[OK] Heap del kernel listo (4MB)\n");

    /* Verificar kmalloc */
    uint32_t* test = (uint32_t*)kmalloc(sizeof(uint32_t) * 4);
    if (test) {
        test[0] = 0xDEAD; test[1] = 0xBEEF;
        test[2] = 0xCAFE; test[3] = 0xBABE;
        kfree(test);
        terminal_print("[OK] kmalloc/kfree funcionando\n");
    }

    kprintf("[OK] PMM inicializado — RAM: %u MB, paginas libres: %u\n\n",
            mem_kb / 1024, pmm_free_pages());

    /* Scheduler */
    scheduler_init();

    /* Configurar TSS con stack de kernel */
    uint32_t tss_stack_top = 0x8000 + 0x1000;
    gdt_set_kernel_stack(tss_stack_top);
    kprintf("[OK] TSS stack del kernel: 0x%x\n", tss_stack_top);

    /* Shell */
    shell_init();

    /* Lanzar proceso init en ring 3 */
    extern void user_init_main(void);
    uint32_t user_eip = (uint32_t)user_init_main;
    uint32_t user_esp = usermode_alloc_stack();

    if (user_esp) {
        uint32_t* test_stack = (uint32_t*)(user_esp - 4);
        *test_stack = 0xDEADBEEF;

        terminal_set_color(COLOR_LCYAN, COLOR_BLACK);
        terminal_print("[OK] Saltando a ring 3 con iret...\n");
        terminal_reset_color();

        extern void test_iret_ring0(void);
        test_iret_ring0();

        __asm__ volatile ("cli");
        jump_to_usermode(user_eip, user_esp);
        /* Llegamos aquí solo si jump_to_usermode retorna (no debería) */
    } else {
        terminal_set_color(COLOR_LRED, COLOR_BLACK);
        terminal_print("[WARN] Sin memoria para stack usuario\n");
        terminal_reset_color();
    }

    /* Restaurar segmentos de kernel y continuar con el shell */
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        : : : "ax"
    );

    terminal_set_color(COLOR_LGREEN, COLOR_BLACK);
    terminal_print("[OK] Proceso init terminado, iniciando shell\n\n");
    terminal_reset_color();

    shell_run();
}

/* kernel.c — Primer código C del kernel */

#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "kheap.h"
#include "scheduler.h"
#include "tss.h"
#include "syscall.h"
#include "vfs.h"
#include "usermode.h"

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

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((volatile uint16_t*)0xB8000)

typedef enum {
    VGA_BLACK=0, VGA_BLUE=1, VGA_GREEN=2, VGA_CYAN=3,
    VGA_RED=4, VGA_MAGENTA=5, VGA_BROWN=6, VGA_LGRAY=7,
    VGA_DGRAY=8, VGA_LBLUE=9, VGA_LGREEN=10, VGA_LCYAN=11,
    VGA_LRED=12, VGA_PINK=13, VGA_YELLOW=14, VGA_WHITE=15,
} vga_color_t;

static size_t  terminal_row;
static size_t  terminal_col;
static uint8_t terminal_color;

static uint8_t vga_make_color(vga_color_t fg, vga_color_t bg) {
    return fg | (bg << 4);
}

static uint16_t vga_make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void terminal_init(void) {
    terminal_row   = 0;
    terminal_col   = 0;
    terminal_color = vga_make_color(VGA_WHITE, VGA_BLACK);
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_make_entry(' ', terminal_color);
}

void terminal_set_color(vga_color_t fg, vga_color_t bg) {
    terminal_color = vga_make_color(fg, bg);
}

static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_make_entry(' ', terminal_color);
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_col = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_scroll();
        return;
    }
    VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_col] = vga_make_entry(c, terminal_color);
    if (++terminal_col == VGA_WIDTH) {
        terminal_col = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_scroll();
    }
}

void terminal_print(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++)
        terminal_putchar(str[i]);
}

static void terminal_print_uint(uint32_t n) {
    if (n == 0) { terminal_putchar('0'); return; }
    char buf[12];
    int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) terminal_putchar(buf[--i]);
}

extern uint32_t _start_kernel;
extern uint32_t _end_kernel;

/* ── Procesos de usuario (definidos antes de kernel_main) ─────────────── */

/* ── Utilidades de string para el shell ───────────────────────────── */
static int kstrncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

static size_t kstrlen2(const char* s) {
    size_t i = 0; while (s[i]) i++; return i;
}

static char cwd[256] = "/";   /* directorio actual */

static void shell_exec(const char* cmd) {
    /* Saltar espacios iniciales */
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    /* help */
    if (kstrncmp(cmd, "help", 4) == 0) {
        terminal_set_color(VGA_LCYAN, VGA_BLACK);
        terminal_print("Comandos disponibles:\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
        terminal_print("  help              mostrar esta ayuda\n");
        terminal_print("  ls [ruta]         listar directorio\n");
        terminal_print("  cat <archivo>     mostrar contenido\n");
        terminal_print("  pwd               directorio actual\n");
        terminal_print("  cd <ruta>         cambiar directorio\n");
        terminal_print("  mkdir <ruta>      crear directorio\n");
        terminal_print("  clear             limpiar pantalla\n");
        terminal_print("  uname             info del sistema\n");
        return;
    }

    /* uname */
    if (kstrncmp(cmd, "uname", 5) == 0) {
        terminal_print("myOS 0.1 x86 monolithic-kernel\n");
        return;
    }

    /* pwd */
    if (kstrncmp(cmd, "pwd", 3) == 0) {
        terminal_print(cwd);
        terminal_putchar('\n');
        return;
    }

    /* clear */
    if (kstrncmp(cmd, "clear", 5) == 0) {
        terminal_init();
        return;
    }

    /* ls */
    if (kstrncmp(cmd, "ls", 2) == 0) {
        const char* path = cwd;
        if (cmd[2] == ' ' && cmd[3]) path = cmd + 3;
        if (vfs_list(path) < 0) {
            terminal_print("ls: no existe: ");
            terminal_print(path);
            terminal_putchar('\n');
        }
        return;
    }

    /* cd */
    if (kstrncmp(cmd, "cd", 2) == 0 && cmd[2] == ' ') {
        const char* path = cmd + 3;
        vfs_node_t* node = vfs_find(path);
        if (node && node->type == VFS_DIRECTORY) {
            size_t len = kstrlen2(path);
            if (len < 255) {
                for (size_t i = 0; i <= len; i++) cwd[i] = path[i];
            }
        } else {
            terminal_print("cd: no existe: ");
            terminal_print(path);
            terminal_putchar('\n');
        }
        return;
    }

    /* cat */
    if (kstrncmp(cmd, "cat", 3) == 0 && cmd[3] == ' ') {
        const char* path = cmd + 4;
        int fd = vfs_open(path);
        if (fd < 0) {
            terminal_print("cat: no existe: ");
            terminal_print(path);
            terminal_putchar('\n');
            return;
        }
        char buf[512];
        int n;
        while ((n = vfs_read(fd, buf, 511)) > 0) {
            buf[n] = '\0';
            terminal_print(buf);
        }
        vfs_close(fd);
        return;
    }

    /* mkdir */
    if (kstrncmp(cmd, "mkdir", 5) == 0 && cmd[5] == ' ') {
        const char* path = cmd + 6;
        if (vfs_mkdir(path) < 0) {
            terminal_print("mkdir: error creando: ");
            terminal_print(path);
            terminal_putchar('\n');
        }
        return;
    }

    /* Comando desconocido */
    terminal_set_color(VGA_LRED, VGA_BLACK);
    terminal_print(cmd);
    terminal_print(": comando no encontrado\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
}

static void shell_process(void) {
    terminal_set_color(VGA_LGREEN, VGA_BLACK);
    terminal_print("\nmyOS shell — escribe 'help' para ver comandos\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);

    char line[256];
    int  line_pos = 0;

    /* Prompt inicial */
    terminal_set_color(VGA_LGREEN, VGA_BLACK);
    terminal_print("user@myos:");
    terminal_set_color(VGA_LBLUE, VGA_BLACK);
    terminal_print(cwd);
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_print("$ ");

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            terminal_putchar('\n');
            line[line_pos] = '\0';
            if (line_pos > 0) shell_exec(line);
            line_pos = 0;

            /* Prompt */
            terminal_set_color(VGA_LGREEN, VGA_BLACK);
            terminal_print("user@myos:");
            terminal_set_color(VGA_LBLUE, VGA_BLACK);
            terminal_print(cwd);
            terminal_set_color(VGA_WHITE, VGA_BLACK);
            terminal_print("$ ");

        } else if (c == '\b') {
            if (line_pos > 0) {
                line_pos--;
                terminal_col--;
                VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_col] =
                    vga_make_entry(' ', vga_make_color(VGA_WHITE, VGA_BLACK));
            }
        } else if (line_pos < 255) {
            line[line_pos++] = c;
            terminal_putchar(c);
        }
    }
}

static void idle_process(void) {
    terminal_set_color(VGA_DGRAY, VGA_BLACK);
    terminal_print("[idle] Proceso idle iniciado (PID 1)\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    while (1) {
        __asm__ volatile ("hlt"); /* esperar interrupciones sin quemar CPU */
    }
}

void kernel_main(uint32_t magic, void* mbi) {

    terminal_init();

    terminal_set_color(VGA_LGREEN, VGA_BLACK);
    terminal_print("myOS v0.1\n");
    terminal_print("=========\n\n");

    terminal_set_color(VGA_WHITE, VGA_BLACK);

    if (magic == 0x36d76289) {
        terminal_print("[OK] Cargado correctamente por GRUB (Multiboot2)\n");
    } else {
        terminal_set_color(VGA_LRED, VGA_BLACK);
        terminal_print("[WARN] Magic number incorrecto\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }

    terminal_print("[OK] Pantalla VGA inicializada\n");
    terminal_print("[OK] Kernel en modo protegido 32-bit\n\n");

    gdt_init();
    terminal_print("[OK] GDT configurada\n");

    idt_init();
    terminal_print("[OK] IDT configurada (32 excepciones + 16 IRQs)\n");

    tss_init();
    terminal_print("[OK] TSS configurado (ring 0/3 listos)\n");

    syscall_init();
    terminal_print("[OK] Syscalls listas (INT 0x80)\n");

    vfs_init();
    terminal_print("[OK] VFS inicializado (/, /bin, /etc, /home, /tmp)\n");

    timer_init(100);
    terminal_print("[OK] Timer PIT inicializado (100 Hz)\n");

    keyboard_init();
    terminal_print("[OK] Teclado PS/2 listo\n");

    __asm__ volatile ("sti");
    terminal_print("[OK] Interrupciones habilitadas\n");

    uint32_t mem_kb = 0;
    if (magic == 0x36d76289 && mbi) {
        mb2_tag_t* tag = (mb2_tag_t*)((uint8_t*)mbi + 8);
        while (tag->type != 0) {
            if (tag->type == 6) {
                mb2_tag_mmap_t*   mmap_tag = (mb2_tag_mmap_t*)tag;
                mb2_mmap_entry_t* entry    = (mb2_mmap_entry_t*)((uint8_t*)mmap_tag + 16);
                uint32_t entries = (mmap_tag->size - 16) / mmap_tag->entry_size;
                for (uint32_t i = 0; i < entries; i++) {
                    if (entry->type == 1) {
                        uint32_t top = (uint32_t)(entry->base_addr + entry->length);
                        if (top / 1024 > mem_kb) mem_kb = top / 1024;
                    }
                    entry = (mb2_mmap_entry_t*)((uint8_t*)entry + mmap_tag->entry_size);
                }
            }
            tag = (mb2_tag_t*)((uint8_t*)tag + ((tag->size + 7) & ~7));
        }
    }
    if (mem_kb == 0) mem_kb = 32 * 1024;

    uint32_t kstart = (uint32_t)&_start_kernel;
    uint32_t kend   = (uint32_t)&_end_kernel;
    pmm_init(mem_kb, kstart, kend);

    paging_init();
    terminal_print("[OK] Paginacion activada (identity map 8MB)\n");

    kheap_init();
    terminal_print("[OK] Heap del kernel listo (4MB)\n");

    /* Prueba de kmalloc */
    uint32_t* test = (uint32_t*)kmalloc(sizeof(uint32_t) * 4);
    if (test) {
        test[0] = 0xDEAD; test[1] = 0xBEEF;
        test[2] = 0xCAFE; test[3] = 0xBABE;
        kfree(test);
        terminal_print("[OK] kmalloc/kfree funcionando\n");
    }

    terminal_print("[OK] PMM inicializado — RAM: ");
    terminal_print_uint(mem_kb / 1024);
    terminal_print(" MB, paginas libres: ");
    terminal_print_uint(pmm_free_pages());
    terminal_print("\n\n");

    /* Inicializar scheduler */
    scheduler_init();

    /* ── Lanzar proceso init en ring 3 ──────────────────────────────── */
    extern void user_init_main(void);

    /* Actualizar TSS con el stack del kernel para cuando lleguen IRQs */
    extern uint8_t _kernel_stack_top[];
    tss_set_kernel_stack((uint32_t)_kernel_stack_top);

    /* Allocar stack de usuario */
    uint32_t user_esp = usermode_alloc_stack();
    if (user_esp) {
        terminal_set_color(VGA_LCYAN, VGA_BLACK);
        terminal_print("[OK] Saltando a ring 3 con iret...\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);

        /* Este iret nos lleva a ring 3 — cuando el proceso init
         * termine via SYS_EXIT, el kernel retoma el control aquí */
        jump_to_usermode((uint32_t)user_init_main, user_esp);
    } else {
        terminal_set_color(VGA_LRED, VGA_BLACK);
        terminal_print("[WARN] Sin memoria para ring 3, corriendo en kernel mode\n");
        terminal_set_color(VGA_WHITE, VGA_BLACK);
    }

    /* Shell del kernel (ring 0) — siempre disponible */
    shell_process();
}
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

    terminal_set_color(VGA_LCYAN, VGA_BLACK);
    terminal_print("myOS shell — escribe algo:\n");
    terminal_set_color(VGA_WHITE, VGA_BLACK);
    terminal_print("> ");

    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            terminal_putchar('\n');
            terminal_print("> ");
        } else if (c == '\b') {
            if (terminal_col > 2) {
                terminal_col--;
                VGA_MEMORY[terminal_row * VGA_WIDTH + terminal_col] =
                    vga_make_entry(' ', vga_make_color(VGA_WHITE, VGA_BLACK));
            }
        } else {
            terminal_putchar(c);
        }
    }
}
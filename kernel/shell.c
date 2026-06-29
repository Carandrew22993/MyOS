/* shell.c — Shell interactivo del kernel
 *
 * Responsabilidades:
 *   - Leer input del teclado caracter a caracter
 *   - Parsear y despachar comandos
 *   - Mantener el estado de sesión (directorio actual)
 *
 * No conoce VGA, no formatea strings directamente.
 * Escribe a través de terminal.h y kprintf.h.
 */

#include "shell.h"
#include "terminal.h"
#include "kprintf.h"
#include "keyboard.h"
#include "vfs.h"
#include "elf.h"
#include "usermode.h"
#include <stdint.h>
#include <stddef.h>

/* ── Estado interno de sesión ──────────────────────────────────────────── */
#define LINE_MAX  256
#define CWD_MAX   256

static char cwd[CWD_MAX];
static char line[LINE_MAX];
static int  line_pos;

/* ── Utilidades de string locales ──────────────────────────────────────── */
static size_t sh_strlen(const char* s) {
    size_t i = 0;
    while (s[i]) i++;
    return i;
}

static int sh_strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

static void sh_strcpy(char* dst, const char* src, size_t max) {
    size_t i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* ── Prompt ────────────────────────────────────────────────────────────── */
static void print_prompt(void) {
    terminal_set_color(COLOR_LGREEN, COLOR_BLACK);
    terminal_print("user@myos:");
    terminal_set_color(COLOR_LBLUE, COLOR_BLACK);
    terminal_print(cwd);
    terminal_reset_color();
    terminal_print("$ ");
}

/* ── Comandos ──────────────────────────────────────────────────────────── */
static void cmd_help(void) {
    terminal_set_color(COLOR_LCYAN, COLOR_BLACK);
    terminal_print("Comandos disponibles:\n");
    terminal_reset_color();
    terminal_print(
        "  help              mostrar esta ayuda\n"
        "  ls [ruta]         listar directorio\n"
        "  cat <archivo>     mostrar contenido\n"
        "  pwd               directorio actual\n"
        "  cd <ruta>         cambiar directorio\n"
        "  mkdir <ruta>      crear directorio\n"
        "  exec <ruta>       ejecutar un ELF desde el VFS\n"
        "  clear             limpiar pantalla\n"
        "  uname             info del sistema\n"
    );
}

static void cmd_ls(const char* path) {
    if (vfs_list(path) < 0) {
        kprintf("ls: no existe: %s\n", path);
    }
}

static void cmd_cat(const char* path) {
    int fd = vfs_open(path);
    if (fd < 0) {
        kprintf("cat: no existe: %s\n", path);
        return;
    }
    char buf[512];
    int  n;
    while ((n = vfs_read(fd, buf, 511)) > 0) {
        buf[n] = '\0';
        terminal_print(buf);
    }
    vfs_close(fd);
}

static void cmd_cd(const char* path) {
    vfs_node_t* node = vfs_find(path);
    if (node && node->type == VFS_DIRECTORY) {
        sh_strcpy(cwd, path, CWD_MAX);
    } else {
        kprintf("cd: no existe: %s\n", path);
    }
}

static void cmd_mkdir(const char* path) {
    if (vfs_mkdir(path) < 0) {
        kprintf("mkdir: error creando: %s\n", path);
    }
}

static void cmd_exec(const char* path) {
    vfs_node_t* node = vfs_find(path);
    if (!node || node->type != VFS_FILE) {
        kprintf("exec: no encontrado: %s\n", path);
        return;
    }
    if (!node->data || node->size < sizeof(elf_header_t)) {
        terminal_print("exec: archivo invalido\n");
        return;
    }

    elf_load_result_t res = elf_load(node->data, node->size);
    if (!res.valid) {
        terminal_print("exec: no es un ELF valido\n");
        return;
    }

    kprintf("Ejecutando: %s\n", path);

    uint32_t user_esp = usermode_alloc_stack();
    if (!user_esp) {
        terminal_print("exec: sin memoria\n");
        return;
    }

    jump_to_usermode(res.entry, user_esp);

    /* Restaurar segmentos de kernel al volver */
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        : : : "ax"
    );
}

/* ── Despachador de comandos ───────────────────────────────────────────── */
static void dispatch(const char* cmd) {
    /* Saltar espacios iniciales */
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;

    if (sh_strncmp(cmd, "help",  4) == 0) { cmd_help(); return; }
    if (sh_strncmp(cmd, "uname", 5) == 0) {
        terminal_print("myOS 0.1 x86 monolithic-kernel\n");
        return;
    }
    if (sh_strncmp(cmd, "pwd",   3) == 0) {
        kprintf("%s\n", cwd);
        return;
    }
    if (sh_strncmp(cmd, "clear", 5) == 0) { terminal_init(); return; }

    if (sh_strncmp(cmd, "ls", 2) == 0) {
        const char* path = (cmd[2] == ' ' && cmd[3]) ? cmd + 3 : cwd;
        cmd_ls(path);
        return;
    }
    if (sh_strncmp(cmd, "cd", 2) == 0 && cmd[2] == ' ') {
        cmd_cd(cmd + 3);
        return;
    }
    if (sh_strncmp(cmd, "cat", 3) == 0 && cmd[3] == ' ') {
        cmd_cat(cmd + 4);
        return;
    }
    if (sh_strncmp(cmd, "mkdir", 5) == 0 && cmd[5] == ' ') {
        cmd_mkdir(cmd + 6);
        return;
    }
    if (sh_strncmp(cmd, "exec", 4) == 0 && cmd[4] == ' ') {
        cmd_exec(cmd + 5);
        return;
    }

    terminal_set_color(COLOR_LRED, COLOR_BLACK);
    kprintf("%s: comando no encontrado\n", cmd);
    terminal_reset_color();
}

/* ── API pública ───────────────────────────────────────────────────────── */
void shell_init(void) {
    sh_strcpy(cwd, "/", CWD_MAX);
    line_pos = 0;
}

void shell_run(void) {
    terminal_set_color(COLOR_LGREEN, COLOR_BLACK);
    terminal_print("\nmyOS shell — escribe 'help' para ver comandos\n");
    terminal_reset_color();

    print_prompt();

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            terminal_putchar('\n');
            line[line_pos] = '\0';
            if (line_pos > 0) dispatch(line);
            line_pos = 0;
            print_prompt();

        } else if (c == '\b') {
            if (line_pos > 0) {
                line_pos--;
                terminal_putchar('\b');
            }
        } else if (line_pos < LINE_MAX - 1) {
            line[line_pos++] = c;
            terminal_putchar(c);
        }
    }
}

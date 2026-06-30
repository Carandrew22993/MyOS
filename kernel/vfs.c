/* vfs.c — Virtual File System (sistema de archivos en memoria)
 *
 * Implementación simple tipo árbol de directorios.
 * Todo vive en RAM — al apagar el sistema se pierde.
 * En el futuro esto se conectará a un driver de disco (ATA/IDE)
 * para persistencia real con FAT32 o ext2.
 *
 * Estructura inicial:
 *   /
 *   ├── bin/
 *   ├── etc/
 *   │   └── hostname
 *   ├── home/
 *   │   └── user/
 *   └── tmp/
 */

#include "vfs.h"
#include "kheap.h"
#include "lib/kstring.h"
#include "lib/kmemory.h"
#include <stdint.h>
#include <stddef.h>

/* ── Almacenamiento ───────────────────────────────────────────────────── */
static vfs_node_t    nodes[VFS_MAX_NODES];
static uint32_t      node_count = 0;
static file_descriptor_t fds[MAX_FD];

/* ── Separar ruta en directorio padre y nombre ────────────────────────── */
static void split_path(const char* path, char* parent_out, char* name_out) {
    size_t len = kstrlen(path);
    int last_slash = -1;

    for (int i = (int)len - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }

    if (last_slash <= 0) {
        /* padre es raíz */
        parent_out[0] = '/'; parent_out[1] = '\0';
        kstrcpy(name_out, path + (last_slash + 1), VFS_NAME_MAX);
    } else {
        kstrcpy(parent_out, path, (size_t)(last_slash + 1));
        parent_out[last_slash] = '\0';
        kstrcpy(name_out, path + last_slash + 1, VFS_NAME_MAX);
    }
}

/* ── Buscar nodo por ruta absoluta ────────────────────────────────────── */
vfs_node_t* vfs_find(const char* path) {
    if (!path || path[0] == '\0') return (void*)0;

    /* Raíz */
    if (path[0] == '/' && path[1] == '\0') return &nodes[0];

    for (uint32_t i = 0; i < node_count; i++) {
        /* Construir ruta completa del nodo */
        char full[VFS_PATH_MAX];
        if (nodes[i].parent == 0) {
            /* hijo directo de raíz */
            full[0] = '/';
            kstrcpy(full + 1, nodes[i].name, VFS_PATH_MAX - 1);
        } else {
            /* reconstruir ruta recursivamente (simplificado 2 niveles) */
            char parent_path[VFS_PATH_MAX];
            vfs_node_t* par = &nodes[nodes[i].parent];
            if (par->parent == 0) {
                parent_path[0] = '/';
                kstrcpy(parent_path + 1, par->name, VFS_PATH_MAX - 1);
            } else {
                parent_path[0] = '/';
                vfs_node_t* gpar = &nodes[par->parent];
                size_t gl = kstrlen(gpar->name);
                parent_path[1] = '/';
                kstrcpy(parent_path + 1, gpar->name, VFS_PATH_MAX - 1);
                size_t pl = kstrlen(parent_path);
                parent_path[pl] = '/';
                kstrcpy(parent_path + pl + 1, par->name, VFS_PATH_MAX - pl - 1);
            }
            size_t pl = kstrlen(parent_path);
            parent_path[pl] = '/';
            kstrcpy(parent_path + pl + 1, nodes[i].name, VFS_PATH_MAX - pl - 1);
            kstrcpy(full, parent_path, VFS_PATH_MAX);
        }
        if (kstrcmp(full, path) == 0) return &nodes[i];
    }
    return (void*)0;
}

/* ── Crear directorio ─────────────────────────────────────────────────── */
int vfs_mkdir(const char* path) {
    if (node_count >= VFS_MAX_NODES) return -1;

    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX];
    split_path(path, parent_path, name);

    vfs_node_t* parent = vfs_find(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY) return -1;

    uint32_t parent_idx = (uint32_t)(parent - nodes);
    uint32_t idx = node_count++;

    kstrcpy(nodes[idx].name, name, VFS_NAME_MAX);
    nodes[idx].type   = VFS_DIRECTORY;
    nodes[idx].size   = 0;
    nodes[idx].data   = (void*)0;
    nodes[idx].parent = parent_idx;
    nodes[idx].inode  = idx;

    return 0;
}

/* ── Crear archivo con contenido ──────────────────────────────────────── */
int vfs_create(const char* path, const char* content) {
    if (node_count >= VFS_MAX_NODES) return -1;

    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX];
    split_path(path, parent_path, name);

    vfs_node_t* parent = vfs_find(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY) return -1;

    uint32_t parent_idx = (uint32_t)(parent - nodes);
    uint32_t idx = node_count++;
    uint32_t len = content ? (uint32_t)kstrlen(content) : 0;

    kstrcpy(nodes[idx].name, name, VFS_NAME_MAX);
    nodes[idx].type   = VFS_FILE;
    nodes[idx].size   = len;
    nodes[idx].parent = parent_idx;
    nodes[idx].inode  = idx;

    if (len > 0) {
        nodes[idx].data = (uint8_t*)kmalloc(len + 1);
        if (nodes[idx].data) {
            kmemcpy(nodes[idx].data, content, len);
            nodes[idx].data[len] = '\0';
        }
    } else {
        nodes[idx].data = (void*)0;
    }

    return 0;
}

/* ── Abrir archivo ────────────────────────────────────────────────────── */
int vfs_open(const char* path) {
    vfs_node_t* node = vfs_find(path);
    if (!node || node->type != VFS_FILE) return -1;

    for (int i = 0; i < MAX_FD; i++) {
        if (!fds[i].used) {
            fds[i].node   = node;
            fds[i].offset = 0;
            fds[i].used   = 1;
            return i;
        }
    }
    return -1; /* sin descriptores libres */
}

/* ── Cerrar archivo ───────────────────────────────────────────────────── */
int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) return -1;
    fds[fd].used = 0;
    return 0;
}

/* ── Leer de un archivo ───────────────────────────────────────────────── */
int vfs_read(int fd, void* buf, size_t len) {
    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) return -1;

    vfs_node_t* node = fds[fd].node;
    if (!node->data) return 0;

    uint32_t available = node->size - fds[fd].offset;
    if (available == 0) return 0;
    if ((uint32_t)len > available) len = available;

    kmemcpy(buf, node->data + fds[fd].offset, len);
    fds[fd].offset += (uint32_t)len;
    return (int)len;
}

/* ── Escribir en un archivo ───────────────────────────────────────────── */
int vfs_write(int fd, const void* buf, size_t len) {
    if (fd < 0 || fd >= MAX_FD || !fds[fd].used) return -1;

    vfs_node_t* node = fds[fd].node;
    uint32_t new_size = fds[fd].offset + (uint32_t)len;

    /* Redimensionar si es necesario */
    if (new_size > node->size) {
        uint8_t* new_data = (uint8_t*)kmalloc(new_size + 1);
        if (!new_data) return -1;
        if (node->data) {
            kmemcpy(new_data, node->data, node->size);
            kfree(node->data);
        }
        node->data = new_data;
        node->size = new_size;
        node->data[new_size] = '\0';
    }

    kmemcpy(node->data + fds[fd].offset, buf, len);
    fds[fd].offset += (uint32_t)len;
    return (int)len;
}

/* ── Listar directorio ────────────────────────────────────────────────── */
extern void terminal_print(const char*);
extern void terminal_putchar(char);
extern void terminal_set_color(int, int);

int vfs_list(const char* path) {
    vfs_node_t* dir = vfs_find(path);
    if (!dir || dir->type != VFS_DIRECTORY) return -1;

    uint32_t dir_idx = (uint32_t)(dir - nodes);

    for (uint32_t i = 0; i < node_count; i++) {
        if (nodes[i].parent == dir_idx && i != 0) {
            if (nodes[i].type == VFS_DIRECTORY) {
                terminal_set_color(9, 0);  /* azul para dirs */
                terminal_print(nodes[i].name);
                terminal_print("/");
            } else {
                terminal_set_color(15, 0); /* blanco para archivos */
                terminal_print(nodes[i].name);
            }
            terminal_putchar('\n');
        }
    }
    terminal_set_color(15, 0);
    return 0;
}

/* ── Inicializar el VFS con estructura base ───────────────────────────── */
void vfs_init(void) {
    /* Limpiar tabla */
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        nodes[i].name[0] = '\0';
        nodes[i].data    = (void*)0;
        nodes[i].size    = 0;
    }
    for (int i = 0; i < MAX_FD; i++) fds[i].used = 0;

    /* Nodo raíz — siempre en índice 0 */
    kstrcpy(nodes[0].name, "/", VFS_NAME_MAX);
    nodes[0].type   = VFS_DIRECTORY;
    nodes[0].parent = 0;
    nodes[0].inode  = 0;
    node_count = 1;

    /* Crear estructura base similar a Linux */
    vfs_mkdir("/bin");
    vfs_mkdir("/etc");
    vfs_mkdir("/home");
    vfs_mkdir("/home/user");
    vfs_mkdir("/tmp");
    vfs_mkdir("/dev");

    /* Archivos iniciales */
    vfs_create("/etc/hostname",  "myos\n");
    vfs_create("/etc/version",   "myOS v0.1 - kernel monolitico x86\n");
    vfs_create("/home/user/readme.txt",
        "Bienvenido a myOS!\n"
        "Este es tu directorio home.\n"
        "Puedes crear archivos con: create <nombre> <contenido>\n");
}

/* Crear archivo binario (para ejecutables ELF embebidos) */
int vfs_create_binary(const char* path, uint8_t* data, uint32_t size) {
    if (node_count >= VFS_MAX_NODES) return -1;

    char parent_path[VFS_PATH_MAX];
    char name[VFS_NAME_MAX];
    split_path(path, parent_path, name);

    vfs_node_t* parent = vfs_find(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY) return -1;

    uint32_t parent_idx = (uint32_t)(parent - nodes);
    uint32_t idx = node_count++;

    kstrcpy(nodes[idx].name, name, VFS_NAME_MAX);
    nodes[idx].type   = VFS_FILE;
    nodes[idx].size   = size;
    nodes[idx].parent = parent_idx;
    nodes[idx].inode  = idx;
    nodes[idx].data   = data;  /* apuntar directamente al array embebido */

    return 0;
}
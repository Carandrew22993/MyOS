#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

/* Tipos de nodo */
#define VFS_FILE      0x01
#define VFS_DIRECTORY 0x02

#define VFS_NAME_MAX  64
#define VFS_PATH_MAX  256
#define VFS_MAX_NODES 128   /* máximo de archivos/dirs en el sistema */

/* Nodo del VFS — representa un archivo o directorio */
typedef struct vfs_node {
    char     name[VFS_NAME_MAX];  /* nombre del archivo o directorio */
    uint32_t type;                /* VFS_FILE o VFS_DIRECTORY */
    uint32_t size;                /* tamaño en bytes (0 para directorios) */
    uint8_t* data;                /* contenido del archivo */
    uint32_t parent;              /* índice del directorio padre */
    uint32_t inode;               /* identificador único */
} vfs_node_t;

/* Descriptor de archivo abierto */
typedef struct {
    vfs_node_t* node;    /* nodo al que apunta */
    uint32_t    offset;  /* posición actual de lectura/escritura */
    uint8_t     used;    /* 1 = descriptor en uso */
} file_descriptor_t;

#define MAX_FD 16   /* máximo de archivos abiertos a la vez */

void        vfs_init(void);
int         vfs_mkdir(const char* path);
int         vfs_create(const char* path, const char* content);
int         vfs_open(const char* path);
int         vfs_close(int fd);
int         vfs_read(int fd, void* buf, size_t len);
int         vfs_write(int fd, const void* buf, size_t len);
int         vfs_list(const char* path);   /* lista el contenido de un dir */
vfs_node_t* vfs_find(const char* path);  /* buscar nodo por ruta */

#endif
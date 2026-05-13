#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* Estructura del header ELF de 32 bits
 * Todo ejecutable ELF empieza con estos bytes en el disco */

#define ELF_MAGIC    0x464C457F  /* 0x7F seguido de 'E','L','F' */
#define ELF_32BIT    1
#define ELF_LITTLE   1
#define ELF_EXEC     2           /* tipo ejecutable */
#define ELF_X86      3           /* arquitectura x86 */

/* Tipos de segmento en el Program Header */
#define PT_NULL      0
#define PT_LOAD      1           /* segmento que se carga en memoria */
#define PT_DYNAMIC   2
#define PT_INTERP    3

/* Flags de segmento */
#define PF_X         0x1         /* ejecutable */
#define PF_W         0x2         /* escribible */
#define PF_R         0x4         /* legible */

typedef struct {
    uint8_t  magic[4];       /* 0x7F E L F */
    uint8_t  bits;           /* 1 = 32bit, 2 = 64bit */
    uint8_t  endian;         /* 1 = little, 2 = big */
    uint8_t  version;        /* siempre 1 */
    uint8_t  abi;
    uint8_t  padding[8];
    uint16_t type;           /* 2 = ejecutable */
    uint16_t machine;        /* 3 = x86 */
    uint32_t version2;
    uint32_t entry;          /* dirección del entry point */
    uint32_t phoff;          /* offset del Program Header Table */
    uint32_t shoff;          /* offset del Section Header Table */
    uint32_t flags;
    uint16_t ehsize;         /* tamaño de este header */
    uint16_t phentsize;      /* tamaño de cada entrada del PH */
    uint16_t phnum;          /* número de entradas del PH */
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf_header_t;

/* Program Header — describe cada segmento cargable */
typedef struct {
    uint32_t type;           /* PT_LOAD = cargar en memoria */
    uint32_t offset;         /* offset en el archivo */
    uint32_t vaddr;          /* dirección virtual destino */
    uint32_t paddr;          /* dirección física (ignorada) */
    uint32_t filesz;         /* tamaño en el archivo */
    uint32_t memsz;          /* tamaño en memoria (puede ser > filesz) */
    uint32_t flags;          /* PF_R, PF_W, PF_X */
    uint32_t align;          /* alineación */
} __attribute__((packed)) elf_phdr_t;

/* Resultado de cargar un ELF */
typedef struct {
    uint32_t entry;          /* dirección del entry point */
    uint32_t load_base;      /* dirección más baja cargada */
    int      valid;          /* 1 si se cargó correctamente */
} elf_load_result_t;

elf_load_result_t elf_load(const uint8_t* data, uint32_t size);
int               elf_validate(const uint8_t* data, uint32_t size);

#endif
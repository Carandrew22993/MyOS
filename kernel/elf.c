/* elf.c — Cargador de ejecutables ELF de 32 bits
 *
 * Proceso de carga:
 * 1. Validar el magic number y los campos del header
 * 2. Iterar sobre los Program Headers buscando segmentos PT_LOAD
 * 3. Para cada segmento: copiar los bytes del archivo a la dirección virtual
 * 4. Poner en cero los bytes extra (BSS)
 * 5. Retornar el entry point para que el kernel salte ahí en ring 3
 */

#include "elf.h"
#include "paging.h"
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>

/* Poner en cero una región de memoria */
static void kzero(void* ptr, uint32_t size) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint32_t i = 0; i < size; i++) p[i] = 0;
}

/* Copiar memoria */
static void kmemcpy(void* dst, const void* src, uint32_t size) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < size; i++) d[i] = s[i];
}

/* Asegurar que una región virtual está mapeada con PAGE_USER */
static void ensure_mapped(uint32_t vaddr, uint32_t size) {
    uint32_t page_start = vaddr & ~0xFFF;
    uint32_t page_end   = (vaddr + size + 0xFFF) & ~0xFFF;

    for (uint32_t page = page_start; page < page_end; page += 0x1000) {
        void* phys = pmm_alloc();
        if (phys) {
            paging_map(page, (uint32_t)phys,
                       PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
    }
}

/* Validar que el buffer es un ELF válido para x86 */
int elf_validate(const uint8_t* data, uint32_t size) {
    if (size < sizeof(elf_header_t)) return 0;

    const elf_header_t* hdr = (const elf_header_t*)data;

    /* Verificar magic */
    if (hdr->magic[0] != 0x7F) return 0;
    if (hdr->magic[1] != 'E')  return 0;
    if (hdr->magic[2] != 'L')  return 0;
    if (hdr->magic[3] != 'F')  return 0;

    if (hdr->bits    != ELF_32BIT)  return 0;
    if (hdr->endian  != ELF_LITTLE) return 0;
    if (hdr->type    != ELF_EXEC)   return 0;
    if (hdr->machine != ELF_X86)    return 0;

    return 1;
}

/* Cargar un ELF en memoria y retornar el entry point */
elf_load_result_t elf_load(const uint8_t* data, uint32_t size) {
    elf_load_result_t result = {0, 0, 0};

    if (!elf_validate(data, size)) return result;

    const elf_header_t* hdr = (const elf_header_t*)data;
    result.entry = hdr->entry;

    /* Iterar sobre los Program Headers */
    for (uint16_t i = 0; i < hdr->phnum; i++) {
        const elf_phdr_t* ph = (const elf_phdr_t*)(
            data + hdr->phoff + i * hdr->phentsize
        );

        /* Solo nos interesan los segmentos cargables */
        if (ph->type != PT_LOAD) continue;
        if (ph->memsz == 0)      continue;

        /* Mapear las páginas necesarias con PAGE_USER */
        ensure_mapped(ph->vaddr, ph->memsz);

        /* Copiar los bytes del archivo a la dirección virtual */
        if (ph->filesz > 0) {
            kmemcpy((void*)ph->vaddr, data + ph->offset, ph->filesz);
        }

        /* Poner en cero el resto (BSS) */
        if (ph->memsz > ph->filesz) {
            kzero((void*)(ph->vaddr + ph->filesz),
                  ph->memsz - ph->filesz);
        }

        /* Registrar la dirección base más baja */
        if (result.load_base == 0 || ph->vaddr < result.load_base) {
            result.load_base = ph->vaddr;
        }
    }

    result.valid = 1;
    return result;
}
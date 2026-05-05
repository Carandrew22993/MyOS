/* linker.ld — Linker script del kernel
 *
 * Le dice al linker cómo acomodar las secciones del binario en memoria.
 * El kernel se carga en la dirección física 1MB (0x100000) por convención.
 * Las primeras secciones deben incluir el header Multiboot2 para que GRUB lo encuentre.
 */

ENTRY(_start)   /* El punto de entrada del binario es _start en boot.asm */

SECTIONS {
    /* Dirección de carga: 1MB — espacio tradicional para kernels x86 */
    . = 1M;

    _start_kernel = .;

    /* La sección multiboot2 va PRIMERO — GRUB busca la firma aquí */
    .multiboot2 : {
        KEEP(*(.multiboot2))
    }

    /* Código ejecutable */
    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    }

    /* Datos solo-lectura (strings constantes, tablas, etc.) */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Datos inicializados */
    .data ALIGN(4K) : {
        *(.data)
        *(.data.*)
    }

    /* Datos sin inicializar — el bootloader los pone en cero */
    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Stack del kernel para el TSS */
    .kstack ALIGN(4K) : {
        _kernel_stack_bottom = .;
        . += 4096;
        _kernel_stack_top = .;
    }

    _end_kernel = .;

    /* Descartar secciones que no necesitamos */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.gcc_except_table*)
    }
}
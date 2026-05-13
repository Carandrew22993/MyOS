/* user.ld — Linker script para programas de usuario
 * Los programas de usuario se cargan en 0x400000 (4MB)
 * — por encima del kernel y fuera del identity map del kernel */

ENTRY(_start)

SECTIONS {
    . = 0x400000;    /* dirección virtual de carga */

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
}
; boot.asm — Entry point del kernel
; GRUB busca la firma Multiboot2 en los primeros 32KB del binario
; y salta a _start con el CPU en modo protegido de 32 bits

MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH     equ 0           ; x86 protegido
HEADER_LENGTH       equ (header_end - header_start)
CHECKSUM            equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + HEADER_LENGTH)

section .multiboot2
header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd HEADER_LENGTH
    dd CHECKSUM
    ; Tag de fin (obligatorio)
    dw 0    ; tipo
    dw 0    ; flags
    dd 8    ; tamaño
header_end:

section .bss
align 16
stack_bottom:
    resb 16384          ; 16 KB para el stack inicial del kernel
stack_top:

section .text
global _start
extern kernel_main      ; definida en kernel.c

_start:
    ; Configurar el stack — el CPU no lo hace por nosotros
    mov esp, stack_top

    ; Limpiar el registro de flags
    push 0
    popf

    ; Llamar al kernel en C
    ; EBX contiene la dirección de la estructura Multiboot2 (info del boot)
    push ebx            ; argumento 2: multiboot_info*
    push eax            ; argumento 1: magic number (debe ser 0x36d76289)
    call kernel_main

    ; Si kernel_main retorna (no debería), colgar el CPU
.hang:
    cli                 ; deshabilitar interrupciones
    hlt                 ; detener el CPU
    jmp .hang           ; por si hlt es interrumpido de alguna forma
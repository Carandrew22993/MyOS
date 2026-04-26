; gdt_asm.asm — Carga la GDT y recarga los registros de segmento
;
; En x86 no basta con ejecutar LGDT — los registros de segmento
; (CS, DS, ES, FS, GS, SS) siguen apuntando a los valores viejos.
; Hay que recargarlos explícitamente.
; CS solo se puede recargar con un far jump.

global gdt_flush

gdt_flush:
    ; El argumento (dirección del gdt_ptr) está en [esp+4]
    mov eax, [esp+4]
    lgdt [eax]          ; cargar la nueva GDT

    ; Recargar registros de datos con el selector 0x10
    ; (entrada 2 de la GDT: índice 2 × 8 bytes = 0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Recargar CS con un far jump al selector 0x08
    ; (entrada 1 de la GDT: índice 1 × 8 bytes = 0x08)
    jmp 0x08:.flush

.flush:
    ret
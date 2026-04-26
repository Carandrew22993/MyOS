; idt_asm.asm — Stubs de interrupción
;
; El CPU al interrumpir empuja automáticamente: SS, ESP, EFLAGS, CS, EIP
; y opcionalmente un código de error. Nosotros completamos el frame
; empujando el número de interrupción y los registros generales.
;
; Luego llamamos al dispatcher en C (isr_handler o irq_handler)
; y restauramos todo al retornar.

global idt_flush

; Declarar todos los stubs como globales
global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

global irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
global irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15

extern isr_handler
extern irq_handler

; ── Macros ──────────────────────────────────────────────────────────────────
; Las excepciones sin código de error empujan un 0 dummy para uniformidad

%macro ISR_NOERR 1
isr%1:
    push dword 0        ; código de error dummy
    push dword %1       ; número de interrupción
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    ; El CPU ya empujó el código de error
    push dword %1       ; número de interrupción
    jmp isr_common
%endmacro

%macro IRQ 2
irq%1:
    push dword 0        ; código de error dummy
    push dword %2       ; número de interrupción (32 + irq)
    jmp irq_common
%endmacro

; ── Stubs de excepciones (ISR 0-31) ─────────────────────────────────────────
; Excepciones CON código de error: 8, 10, 11, 12, 13, 14, 17
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; ── Stubs de IRQs de hardware (remapeadas a 32-47) ───────────────────────────
IRQ  0, 32
IRQ  1, 33
IRQ  2, 34
IRQ  3, 35
IRQ  4, 36
IRQ  5, 37
IRQ  6, 38
IRQ  7, 39
IRQ  8, 40
IRQ  9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ── Rutina común para excepciones ───────────────────────────────────────────
isr_common:
    pusha               ; empujar EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    mov ax, ds
    push eax            ; guardar segmento de datos

    mov ax, 0x10        ; cargar segmento de datos del kernel
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp            ; pasar puntero al frame como argumento
    call isr_handler
    add esp, 4

    pop eax             ; restaurar segmento de datos original
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8          ; limpiar int_no y err_code del stack
    iret                ; retornar de la interrupción

; ── Rutina común para IRQs ──────────────────────────────────────────────────
irq_common:
    pusha
    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iret

; ── Cargar la IDT ───────────────────────────────────────────────────────────
idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret
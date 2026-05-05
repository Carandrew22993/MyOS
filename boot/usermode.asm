; usermode.asm — Salto a ring 3 mediante iret
;
; jump_to_usermode(eip, esp) hace:
;   1. Configura los segmentos de datos de usuario (0x23)
;   2. Empuja al stack los 5 valores que iret necesita
;   3. Ejecuta iret — el CPU cambia a ring 3
;
; Selectores:
;   0x1B = GDT entrada 3, RPL 3 (código usuario)
;   0x23 = GDT entrada 4, RPL 3 (datos usuario)

global jump_to_usermode

jump_to_usermode:
    ; Argumentos en el stack:
    ;   [esp+4] = eip (entry point del proceso)
    ;   [esp+8] = esp_user (stack del proceso usuario)

    mov eax, [esp+4]    ; eip
    mov ecx, [esp+8]    ; esp usuario

    ; Cargar segmentos de datos de usuario
    mov dx, 0x23        ; selector datos usuario (ring 3)
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ; Construir el frame de iret en el stack:
    push 0x23           ; SS usuario
    push ecx            ; ESP usuario
    push 0x202          ; EFLAGS: IF=1 (bit 9) + reservado (bit 1)
    push 0x1B           ; CS usuario (ring 3)
    push eax            ; EIP (entry point)

    iret                ; saltar a ring 3 — no retorna
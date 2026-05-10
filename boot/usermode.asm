; usermode.asm — Salto a ring 3 mediante iret
global jump_to_usermode
global test_iret_ring0

jump_to_usermode:
    mov eax, [esp+4]    ; eip usuario
    mov ecx, [esp+8]    ; esp usuario

    mov dx, 0x23
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    push dword 0x23     ; SS usuario
    push ecx            ; ESP usuario
    push dword 0x202    ; EFLAGS: IF=1
    push dword 0x1B     ; CS usuario
    push eax            ; EIP usuario
    iret

test_iret_ring0:
    pushfd
    push dword 0x08
    push dword .ret
    iret
.ret:
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
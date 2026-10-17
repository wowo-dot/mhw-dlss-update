PUBLIC mhw_binding_forward

SAVE MACRO where
    mov [where], rax
    mov [where+8], rcx
    mov [where+10h], rdx
    mov [where+18h], r8
    mov [where+20h], r9
    mov [where+28h], r10
    mov [where+30h], r11
    pushfq
    pop rax
    mov [where+38h], rax
    movdqu [where+40h], xmm0
    movdqu [where+50h], xmm1
    movdqu [where+60h], xmm2
    movdqu [where+70h], xmm3
    movdqu [where+80h], xmm4
    movdqu [where+90h], xmm5
ENDM
RESTORE MACRO where
    movdqu xmm0, [where+40h]
    movdqu xmm1, [where+50h]
    movdqu xmm2, [where+60h]
    movdqu xmm3, [where+70h]
    movdqu xmm4, [where+80h]
    movdqu xmm5, [where+90h]
    push qword ptr [where+38h]
    popfq
    mov rax, [where]
    mov rcx, [where+8]
    mov rdx, [where+10h]
    mov r8, [where+18h]
    mov r9, [where+20h]
    mov r10, [where+28h]
    mov r11, [where+30h]
ENDM
.code
; A described unwind frame encloses the original call. The C++ dispatcher
; supplies __finally cleanup if that original raises. No return-address rewrite.
mhw_binding_forward PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    mov rbx, rdx
    mov rsi, rcx
    mov rax, [rbx+0A8h]
    mov [rsp+20h], rax
    mov rax, [rbx+0B0h]
    mov [rsp+28h], rax
    mov rax, [rbx+0B8h]
    mov [rsp+30h], rax
    mov rax, [rbx+0C0h]
    mov [rsp+38h], rax
    RESTORE rbx
    call rsi
    SAVE rbx
    lea rsp, [rsp+48h]
    pop rsi
    pop rbx
    ret
mhw_binding_forward ENDP
END

PUBLIC mhw_binding_forward

INCLUDE native_frame.inc
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

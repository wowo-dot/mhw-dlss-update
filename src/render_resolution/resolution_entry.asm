EXTERN mhw_resolution_dispatch:PROC
PUBLIC mhw_resolution_entry
.code
mhw_resolution_entry PROC FRAME
    sub rsp, 0F8h
    .allocstack 0F8h
    .endprolog
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    pushfq
    pop rax
    mov [rsp+58h], rax
    movdqu [rsp+60h], xmm0
    movdqu [rsp+70h], xmm1
    movdqu [rsp+80h], xmm2
    movdqu [rsp+90h], xmm3
    movdqu [rsp+0A0h], xmm4
    movdqu [rsp+0B0h], xmm5
    mov rax, [rsp+0F8h]
    mov [rsp+0C0h], rax
    xor eax, eax
    mov [rsp+0C8h], rax
    mov [rsp+0D0h], rax
    mov [rsp+0D8h], rax
    mov [rsp+0E0h], rax
    lea rcx, [rsp+20h]
    call mhw_resolution_dispatch
    movdqu xmm0, [rsp+60h]
    movdqu xmm1, [rsp+70h]
    movdqu xmm2, [rsp+80h]
    movdqu xmm3, [rsp+90h]
    movdqu xmm4, [rsp+0A0h]
    movdqu xmm5, [rsp+0B0h]
    push qword ptr [rsp+58h]
    popfq
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
    lea rsp, [rsp+0F8h]
    ret
mhw_resolution_entry ENDP
END

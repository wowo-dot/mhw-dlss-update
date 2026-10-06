EXTERN mhw_reflex_boundary_dispatch:PROC
PUBLIC mhw_reflex_boundary_forward

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
ENTRY MACRO name, kind
PUBLIC name
name PROC FRAME
    sub rsp, 118h
    .allocstack 118h
    .endprolog
    SAVE rsp+40h
    mov rax, [rsp+118h]
    mov [rsp+0E0h], rax
    mov ecx, kind
    lea rdx, [rsp+40h]
    call mhw_reflex_boundary_dispatch
    RESTORE rsp+40h
    lea rsp, [rsp+118h]
    ret
name ENDP
ENDM
.code
; No probed function has stack arguments. Describe unwind frames and preserve
; all volatile integer/vector arguments and returns across the original call.
mhw_reflex_boundary_forward PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov rbx, rdx
    mov rsi, rcx
    RESTORE rbx
    call rsi
    SAVE rbx
    lea rsp, [rsp+28h]
    pop rsi
    pop rbx
    ret
mhw_reflex_boundary_forward ENDP
ENTRY reflex_boundary_update, 0
ENTRY reflex_boundary_advance, 1
ENTRY reflex_boundary_publish, 2
ENTRY reflex_boundary_render, 3
ENTRY reflex_boundary_native_present, 4
ENTRY reflex_boundary_execute, 5
ENTRY reflex_boundary_present, 6
ENTRY reflex_boundary_keyboard, 7
ENTRY reflex_boundary_mouse, 8
ENTRY reflex_boundary_devices, 9
ENTRY reflex_boundary_jobs, 10
ENTRY reflex_boundary_submit, 11
ENTRY reflex_boundary_device_poll, 12
END

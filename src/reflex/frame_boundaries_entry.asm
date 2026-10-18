EXTERN mhw_reflex_boundary_dispatch:PROC
PUBLIC mhw_reflex_boundary_forward

INCLUDE native_frame.inc
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

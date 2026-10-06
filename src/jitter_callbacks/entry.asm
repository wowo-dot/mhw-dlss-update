EXTERN mhw_jitter_before:PROC
EXTERN mhw_jitter_after:PROC
EXTERN mhw_jitter_originals:QWORD
PUBLIC mhw_jitter_update_entry
PUBLIC mhw_jitter_reset_entry
PUBLIC mhw_jitter_setter_entry

SAVE_VOLATILES MACRO
    mov [rsp+20h], rax
    mov [rsp+28h], rcx
    mov [rsp+30h], rdx
    mov [rsp+38h], r8
    mov [rsp+40h], r9
    mov [rsp+48h], r10
    mov [rsp+50h], r11
    movdqu [rsp+60h], xmm0
    movdqu [rsp+70h], xmm1
    movdqu [rsp+80h], xmm2
    movdqu [rsp+90h], xmm3
    movdqu [rsp+0A0h], xmm4
    movdqu [rsp+0B0h], xmm5
    pushfq
    pop rax
    mov [rsp+0C0h], rax
ENDM
RESTORE_VOLATILES MACRO
    push qword ptr [rsp+0C0h]
    popfq
    movdqu xmm0, [rsp+60h]
    movdqu xmm1, [rsp+70h]
    movdqu xmm2, [rsp+80h]
    movdqu xmm3, [rsp+90h]
    movdqu xmm4, [rsp+0A0h]
    movdqu xmm5, [rsp+0B0h]
    mov rax, [rsp+20h]
    mov rcx, [rsp+28h]
    mov rdx, [rsp+30h]
    mov r8, [rsp+38h]
    mov r9, [rsp+40h]
    mov r10, [rsp+48h]
    mov r11, [rsp+50h]
ENDM
WRAPPER MACRO name, kind, target_offset
name PROC FRAME
    sub rsp, 0D8h
    .allocstack 0D8h
    .endprolog
    SAVE_VOLATILES
    mov ecx, kind
    mov rdx, [rsp+28h]
    mov r8, [rsp+30h]
    mov r9, [rsp+0D8h]
    call mhw_jitter_before
    mov [rsp+0C8h], rax
    RESTORE_VOLATILES
    call qword ptr [mhw_jitter_originals+target_offset]
    SAVE_VOLATILES
    mov rcx, [rsp+0C8h]
    call mhw_jitter_after
    RESTORE_VOLATILES
    lea rsp, [rsp+0D8h]
    ret
name ENDP
ENDM
.code
WRAPPER mhw_jitter_update_entry, 0, 0
WRAPPER mhw_jitter_reset_entry, 1, 8
WRAPPER mhw_jitter_setter_entry, 2, 10h
END

EXTERN mhw_aa_dispatch:PROC
EXTERN mhw_aa_context:QWORD
EXTERN mhw_aa_thread:QWORD
EXTERN mhw_aa_table_forced:QWORD
EXTERN mhw_aa_sharpen_suppressed:QWORD
EXTERN mhw_aa_table_original:QWORD
EXTERN mhw_aa_sharpen_original:QWORD
EXTERN mhw_aa_table_resume:QWORD
EXTERN mhw_aa_sharpen_resume:QWORD

ENTRY MACRO name, kind
PUBLIC name
name PROC FRAME
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
    mov ecx, kind
    lea rdx, [rsp+20h]
    call mhw_aa_dispatch
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
name ENDP
ENDM
.code
ENTRY mhw_aa_frame_entry, 0
ENTRY mhw_aa_taa_entry, 1
ENTRY mhw_aa_queue_entry, 7
ENTRY mhw_aa_jitter2_entry, 2
ENTRY mhw_aa_jitter1_entry, 3
ENTRY mhw_aa_jitter3_entry, 4

; Each site replaces one seven-byte CMP and resumes before the native branch.
; No calls, stack arguments, shared settings writes or GPR/vector changes.
PUBLIC mhw_aa_table_entry
mhw_aa_table_entry PROC
    cmp rdx, qword ptr [mhw_aa_context]
    jne table_native
    test rdx, rdx
    jz table_native
    push rax
    mov rax, gs:[30h] ; NtCurrentTeb(): scope must belong to this render thread.
    cmp rax, qword ptr [mhw_aa_thread]
    pop rax
    jne table_native
    lock inc qword ptr [mhw_aa_table_forced]
    test rdx, rdx ; ZF=0: temporal preparation is enabled in the owned context.
    jmp qword ptr [mhw_aa_table_resume]
table_native:
    jmp qword ptr [mhw_aa_table_original]
mhw_aa_table_entry ENDP

PUBLIC mhw_aa_sharpen_entry
mhw_aa_sharpen_entry PROC
    push rax
    mov rax, qword ptr [mhw_aa_context]
    test rax, rax
    jz sharpen_native
    cmp qword ptr [rbp+98h], rax
    jne sharpen_native
    mov rax, gs:[30h]
    cmp rax, qword ptr [mhw_aa_thread]
    jne sharpen_native
    pop rax
    lock inc qword ptr [mhw_aa_sharpen_suppressed]
    cmp rsp, rsp ; ZF=1: native branch selects current ring, not sharpened next ring.
    jmp qword ptr [mhw_aa_sharpen_resume]
sharpen_native:
    pop rax
    jmp qword ptr [mhw_aa_sharpen_original]
mhw_aa_sharpen_entry ENDP
END

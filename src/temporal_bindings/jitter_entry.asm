EXTERN mhw_temporal_jitter:PROC


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
ENTRY MACRO name, handler, stack_count
PUBLIC name
name PROC FRAME
    sub rsp, 138h
    .allocstack 138h
    .endprolog
    SAVE rsp+40h
    mov rax, [rsp+138h]
    mov [rsp+0E0h], rax
    xor eax, eax
    mov [rsp+0E8h], rax
    mov [rsp+0F0h], rax
    mov [rsp+0F8h], rax
    mov [rsp+100h], rax
IF stack_count GE 1
    mov rax, [rsp+160h]
    mov [rsp+0E8h], rax
ENDIF
IF stack_count GE 2
    mov rax, [rsp+168h]
    mov [rsp+0F0h], rax
ENDIF
IF stack_count GE 3
    mov rax, [rsp+170h]
    mov [rsp+0F8h], rax
ENDIF
IF stack_count GE 4
    mov rax, [rsp+178h]
    mov [rsp+100h], rax
ENDIF
    mov [rsp+108h], rbx
    lea rcx, [rsp+40h]
    call handler
    RESTORE rsp+40h
    lea rsp, [rsp+138h]
    ret
name ENDP
ENDM
.code
ENTRY temporal_jitter_entry, mhw_temporal_jitter, 0
END

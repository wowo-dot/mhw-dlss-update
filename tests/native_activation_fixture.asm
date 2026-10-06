PUBLIC activation_table_fixture
PUBLIC activation_table_site
PUBLIC activation_table_resume
PUBLIC activation_sharpen_fixture
PUBLIC activation_sharpen_site
PUBLIC activation_sharpen_resume
.code
; Owned minimal branch fixtures, not copied game functions.
activation_table_fixture PROC
    mov rax, rcx
activation_table_site LABEL BYTE
    cmp byte ptr [rax+0E9E4h], 0
activation_table_resume LABEL BYTE
    je table_off
    mov eax, 1
    ret
table_off:
    xor eax, eax
    ret
activation_table_fixture ENDP
activation_sharpen_fixture PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 0B8h
    .allocstack 0B8h
    .endprolog
    mov rbp, rsp
    mov rbx, rcx
    mov [rbp+98h], rdx
activation_sharpen_site LABEL BYTE
    cmp byte ptr [rbx+187h], 0
activation_sharpen_resume LABEL BYTE
    mov rax, 100
    je sharpen_off
    mov eax, 1
    jmp sharpen_done
sharpen_off:
    xor eax, eax
sharpen_done:
    lea rsp, [rbp+0B8h]
    pop rbx
    pop rbp
    ret
activation_sharpen_fixture ENDP
END

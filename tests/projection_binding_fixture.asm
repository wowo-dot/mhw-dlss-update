EXTERN binding_fixture_body:PROC
PUBLIC binding_fixture_returns
PUBLIC binding_fixture_unsafe
FENTRY MACRO name, kind
PUBLIC name
name PROC FRAME
    sub rsp, 58h
    .allocstack 58h
    .endprolog
    ; Keep calls out of MinHook's relocated prefix for valid unwind metadata.
    REPT 16
    nop
    ENDM
    mov rax, [rsp+80h]
    mov [rsp+20h], rax
    mov rax, [rsp+88h]
    mov [rsp+28h], rax
    mov rax, [rsp+90h]
    mov [rsp+30h], rax
    mov qword ptr [rsp+38h], kind
    call binding_fixture_body
    movq xmm0, rax
    add rsp, 58h
    ret
name ENDP
ENDM
.code
binding_fixture_unsafe PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call binding_fixture0
    add rsp, 28h
    ret
binding_fixture_unsafe ENDP
FENTRY binding_fixture0, 0
FENTRY binding_fixture1, 1
FENTRY binding_fixture2, 2
FENTRY binding_fixture3, 3
FENTRY binding_fixture4, 4
FENTRY binding_fixture5, 5
FENTRY binding_fixture6, 6
FENTRY binding_fixture7, 7
FENTRY binding_fixture8, 8
FENTRY binding_fixture9, 9
FENTRY binding_fixture10, 10
FENTRY binding_fixture11, 11
FENTRY binding_fixture12, 12
FENTRY binding_fixture13, 13
FENTRY binding_fixture14, 14
FENTRY binding_fixture15, 15
FENTRY binding_fixture16, 16
FENTRY binding_fixture17, 17
FENTRY binding_fixture18, 18
FENTRY binding_fixture19, 19
FENTRY binding_fixture20, 20
FENTRY binding_fixture21, 21
FENTRY binding_fixture22, 22
FENTRY binding_fixture23, 23
FENTRY binding_fixture24, 24
FENTRY binding_fixture25, 25
FENTRY binding_fixture26, 26
FENTRY binding_fixture27, 27
; RCX function, RDX two-word output. Exercise all seven indirect-call args.
binding_fixture_returns PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    mov rbx, rdx
    mov rax, rcx
    mov rcx, 11h
    mov rdx, 22h
    mov r8, 33h
    mov r9, 44h
    mov qword ptr [rsp+20h], 55h
    mov qword ptr [rsp+28h], 66h
    mov qword ptr [rsp+30h], 77h
    call rax
    mov [rbx], rax
    movq qword ptr [rbx+8], xmm0
    add rsp, 40h
    pop rbx
    ret
binding_fixture_returns ENDP
END

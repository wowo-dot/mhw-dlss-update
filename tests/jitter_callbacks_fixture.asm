EXTERN jitter_fixture_setter_body:PROC
PUBLIC jitter_fixture_setter
PUBLIC jitter_fixture_reset
PUBLIC jitter_fixture_update
PUBLIC jitter_fixture_returns
EXTERN jitter_fixture_update_body:PROC
EXTERN jitter_fixture_reset_body:PROC
.code
; Owned trampolines exercise a normal nested call and a tail-call path.
jitter_fixture_setter PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call jitter_fixture_setter_body
    mov rax, 1122334455667788h
    movq xmm0, rax
    add rsp, 28h
    ret
jitter_fixture_setter ENDP
jitter_fixture_update PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call jitter_fixture_update_body
    add rsp, 28h
    ret
jitter_fixture_update ENDP
jitter_fixture_reset PROC
    nop
    nop
    nop
    nop
    nop
    jmp jitter_fixture_reset_body
jitter_fixture_reset ENDP
jitter_fixture_returns PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov [rsp+20h], r8
    mov [rsp+28h], r9
    call jitter_fixture_setter
    mov r8, [rsp+20h]
    mov [r8], rax
    mov r9, [rsp+28h]
    movq qword ptr [r9], xmm0
    add rsp, 38h
    ret
jitter_fixture_returns ENDP
END

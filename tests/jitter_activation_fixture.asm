EXTERN activation_fixture_body:PROC
PUBLIC activation_fixture
.code
activation_fixture PROC FRAME
    ; Match the mapped target's stack-prologue shape with our own test body.
    db 40h
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    lea rbp, [rsp - 1a0h]
    sub rsp, 2a0h
    .allocstack 2a0h
    .endprolog
    call activation_fixture_body
    add rsp, 2a0h
    pop rdi
    pop rbx
    pop rbp
    ret
activation_fixture ENDP
END

EXTERN writer_fixture_body:PROC
PUBLIC writer_fixture_setter
PUBLIC writer_fixture_returns
PUBLIC writer_fixture_return_site
.code
writer_fixture_setter PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call writer_fixture_body
    mov rax, 1122334455667788h
    movq xmm0, rax
    add rsp, 28h
    ret
writer_fixture_setter ENDP
writer_fixture_returns PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov [rsp+20h], r8
    mov [rsp+28h], r9
    call writer_fixture_setter
writer_fixture_return_site LABEL BYTE
    mov r8, [rsp+20h]
    mov [r8], rax
    mov r9, [rsp+28h]
    movq qword ptr [r9], xmm0
    add rsp, 38h
    ret
writer_fixture_returns ENDP
END

EXTERN scaled_downstream_entry:PROC
PUBLIC downstream_fixture_call
PUBLIC downstream_fixture_return
.code
downstream_fixture_call PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov rax, [rsp+60h]
    mov [rsp+20h], rax
    mov rax, [rsp+68h]
    mov [rsp+28h], rax
    call scaled_downstream_entry
downstream_fixture_return::
    add rsp, 38h
    ret
downstream_fixture_call ENDP
END

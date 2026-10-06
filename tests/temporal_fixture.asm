EXTERN temporal_dispatch_entry:PROC
PUBLIC temporal_fixture_call
.code
temporal_fixture_call PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 30h
    .allocstack 30h
    .endprolog
    mov rbx, rcx
    mov rcx, 1122h
    mov rdx, 3344h
    mov r8, 5566h
    mov r9, 7788h
    mov qword ptr [rsp+20h], 99aah
    mov qword ptr [rsp+28h], 0aabbh
    call temporal_dispatch_entry
    add rsp, 30h
    pop rbx
    ret
temporal_fixture_call ENDP
END

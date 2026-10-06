EXTERN aa_schedule_entry:PROC
PUBLIC post_queue_fixture_call
.code
post_queue_fixture_call PROC FRAME
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
    call aa_schedule_entry
    add rsp, 30h
    pop rbx
    ret
post_queue_fixture_call ENDP
END

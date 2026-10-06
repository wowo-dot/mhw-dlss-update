EXTERN mhw_depth_fixture_helper:PROC
EXTERN mhw_depth_fixture_consumer:PROC
PUBLIC mhw_depth_fixture_prepare
PUBLIC mhw_depth_fixture_color_return
PUBLIC mhw_depth_fixture_motion_return
PUBLIC mhw_depth_fixture_depth_return
PUBLIC mhw_depth_fixture_output_return
PUBLIC mhw_depth_fixture_consume
PUBLIC mhw_depth_fixture_consumer_return
.code
mhw_depth_fixture_prepare PROC FRAME
    push rbx
    .pushreg rbx
    push r15
    .pushreg r15
    push r12
    .pushreg r12
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    mov r12, rcx
    mov rbx, rdx
    mov r15, r8
    mov rdx, [rbx+8]
    mov rdx, [rdx+60h]
    mov r8d, 4
    call mhw_depth_fixture_helper
mhw_depth_fixture_color_return LABEL BYTE
    mov rcx, r12
    mov rdx, [rbx+10h]
    mov rdx, [rdx+60h]
    mov r8d, 4
    call mhw_depth_fixture_helper
mhw_depth_fixture_motion_return LABEL BYTE
    mov rcx, r12
    mov rdx, [rbx+20h]
    mov rdx, [rdx+0f0h]
    mov r8d, 4
    call mhw_depth_fixture_helper
mhw_depth_fixture_depth_return LABEL BYTE
    mov rcx, r12
    mov rdx, [rbx+18h]
    mov rdx, [rdx+60h]
    mov r8d, 400h
    call mhw_depth_fixture_helper
mhw_depth_fixture_output_return LABEL BYTE
    add rsp, 20h
    pop r12
    pop r15
    pop rbx
    ret
mhw_depth_fixture_prepare ENDP
mhw_depth_fixture_consume PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call mhw_depth_fixture_consumer
mhw_depth_fixture_consumer_return LABEL BYTE
    add rsp, 28h
    ret
mhw_depth_fixture_consume ENDP
END

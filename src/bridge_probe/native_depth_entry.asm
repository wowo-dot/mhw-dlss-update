EXTERN native_depth_helper_dispatch:PROC
PUBLIC native_depth_helper_entry
.code
native_depth_helper_entry PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov r9, [rsp+38h]
    mov [rsp+20h], rbx
    mov [rsp+28h], r15
    call native_depth_helper_dispatch
    add rsp, 38h
    ret
native_depth_helper_entry ENDP
END

option casemap:none
extern fixture_native_calls:DWORD
extern fixture_arguments:QWORD
extern fixture_native_result:DWORD
public fixture_native
public fixture_invoke
public fixture_return
.code
fixture_native proc
    mov qword ptr [fixture_arguments], rcx
    mov qword ptr [fixture_arguments+8], rdx
    mov qword ptr [fixture_arguments+16], r8
    mov qword ptr [fixture_arguments+24], r9
    inc dword ptr [fixture_native_calls]
    mov eax, dword ptr [fixture_native_result]
    ret
fixture_native endp
fixture_invoke proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call fixture_native
fixture_return::
    add rsp, 28h
    ret
fixture_invoke endp
end

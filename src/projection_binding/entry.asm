EXTERN mhw_binding_dispatch:PROC
PUBLIC mhw_binding_forward

SAVE MACRO where
    mov [where], rax
    mov [where+8], rcx
    mov [where+10h], rdx
    mov [where+18h], r8
    mov [where+20h], r9
    mov [where+28h], r10
    mov [where+30h], r11
    pushfq
    pop rax
    mov [where+38h], rax
    movdqu [where+40h], xmm0
    movdqu [where+50h], xmm1
    movdqu [where+60h], xmm2
    movdqu [where+70h], xmm3
    movdqu [where+80h], xmm4
    movdqu [where+90h], xmm5
ENDM
RESTORE MACRO where
    movdqu xmm0, [where+40h]
    movdqu xmm1, [where+50h]
    movdqu xmm2, [where+60h]
    movdqu xmm3, [where+70h]
    movdqu xmm4, [where+80h]
    movdqu xmm5, [where+90h]
    push qword ptr [where+38h]
    popfq
    mov rax, [where]
    mov rcx, [where+8]
    mov rdx, [where+10h]
    mov r8, [where+18h]
    mov r9, [where+20h]
    mov r10, [where+28h]
    mov r11, [where+30h]
ENDM
ENTRY MACRO name, kind, stack_count
PUBLIC name
name PROC FRAME
    sub rsp, 118h
    .allocstack 118h
    .endprolog
    SAVE rsp+40h
    mov rax, [rsp+118h]
    mov [rsp+0E0h], rax
    xor eax, eax
    mov [rsp+0E8h], rax
    mov [rsp+0F0h], rax
    mov [rsp+0F8h], rax
    mov [rsp+100h], rax
IF stack_count GE 1
    mov rax, [rsp+140h]
    mov [rsp+0E8h], rax
ENDIF
IF stack_count GE 2
    mov rax, [rsp+148h]
    mov [rsp+0F0h], rax
ENDIF
IF stack_count GE 3
    mov rax, [rsp+150h]
    mov [rsp+0F8h], rax
ENDIF
IF stack_count GE 4
    mov rax, [rsp+158h]
    mov [rsp+100h], rax
ENDIF
    mov ecx, kind
    lea rdx, [rsp+40h]
    call mhw_binding_dispatch
    RESTORE rsp+40h
    lea rsp, [rsp+118h]
    ret
name ENDP
ENDM
.code
; A described unwind frame encloses the original call. The C++ dispatcher
; supplies __finally cleanup if that original raises. No return-address rewrite.
mhw_binding_forward PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    mov rbx, rdx
    mov rsi, rcx
    mov rax, [rbx+0A8h]
    mov [rsp+20h], rax
    mov rax, [rbx+0B0h]
    mov [rsp+28h], rax
    mov rax, [rbx+0B8h]
    mov [rsp+30h], rax
    mov rax, [rbx+0C0h]
    mov [rsp+38h], rax
    RESTORE rbx
    call rsi
    SAVE rbx
    lea rsp, [rsp+48h]
    pop rsi
    pop rbx
    ret
mhw_binding_forward ENDP
ENTRY binding_producer, 0, 0
ENTRY binding_publish, 1, 0
ENTRY binding_consume, 2, 0
ENTRY binding_state, 3, 0
ENTRY binding_draw0, 4, 0
ENTRY binding_draw1, 5, 0
ENTRY binding_draw2, 6, 0
ENTRY binding_draw3, 7, 0
ENTRY binding_draw4, 8, 0
ENTRY binding_root_cbv, 9, 0
ENTRY binding_root_table, 10, 0
ENTRY binding_root_signature, 11, 0
ENTRY binding_pipeline, 12, 0
ENTRY binding_viewport, 13, 0
ENTRY binding_api_draw, 14, 1
ENTRY binding_api_indexed, 15, 2
ENTRY binding_api_indirect, 16, 3
ENTRY binding_create_cbv, 17, 0
ENTRY binding_reset, 18, 0
ENTRY binding_rollover, 19, 0
ENTRY binding_outputs, 20, 1
ENTRY binding_barriers, 21, 0
ENTRY binding_create_pipeline, 22, 0
ENTRY binding_scissor, 23, 0
ENTRY provenance_construct, 24, 0
ENTRY provenance_destroy, 25, 0
ENTRY provenance_dsv, 26, 0
ENTRY provenance_copy, 27, 4
ENTRY provenance_copy_simple, 28, 1
ENTRY provenance_close, 29, 0
ENTRY provenance_clear, 30, 3
ENTRY provenance_bundle, 31, 0
ENTRY provenance_execute, 32, 0
ENTRY provenance_queue_signal, 33, 0
ENTRY provenance_queue_wait, 34, 0
ENTRY provenance_fence_completed, 35, 0
ENTRY provenance_fence_signal, 36, 0
ENTRY provenance_copy_texture, 37, 3
ENTRY provenance_copy_resource, 38, 0
ENTRY provenance_packet_producer, 39, 1
ENTRY provenance_packet_consumer, 40, 0
END

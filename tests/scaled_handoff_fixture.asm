EXTERN scaled_handoff_entry:PROC
PUBLIC handoff_fixture_call
PUBLIC handoff_fixture_return
.code
handoff_fixture_call PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call scaled_handoff_entry
handoff_fixture_return::
    add rsp, 28h
    ret
handoff_fixture_call ENDP
END

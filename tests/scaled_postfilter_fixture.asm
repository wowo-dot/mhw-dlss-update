EXTERN scaled_postfilter_entry:PROC
PUBLIC postfilter_fixture_call
PUBLIC postfilter_fixture_return
.code
postfilter_fixture_call PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call scaled_postfilter_entry
postfilter_fixture_return::
    add rsp, 28h
    ret
postfilter_fixture_call ENDP
END

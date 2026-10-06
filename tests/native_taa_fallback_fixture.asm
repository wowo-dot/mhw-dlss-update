EXTERN mhw_taa_fixture_producer:PROC
PUBLIC mhw_taa_fixture_call
PUBLIC mhw_taa_fixture_return
.code
mhw_taa_fixture_call PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    call mhw_taa_fixture_producer
mhw_taa_fixture_return LABEL BYTE
    add rsp, 28h
    ret
mhw_taa_fixture_call ENDP
END

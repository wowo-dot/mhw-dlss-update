; Owned-child self-test only. Retrying the unchanged load must return the
; backing's tracker value. No generated code or remote executable writes.
.code
mhw_depth_fixture_fault PROC
    mov rax, [rdx+70h]
    ret
mhw_depth_fixture_fault ENDP
END

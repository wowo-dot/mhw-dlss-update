# Prepared frame output: first runtime failure

2026-09-30, v1.25.17, PID 15060 / creation 134352126976240901.
Evidence: `local/frame-output-20260930`.

Startup, modern SDK capability publication and executable/DLL identity passed.
The main-menu test produced one successful DLSS frame. The stream route latched
fault 8 on the next prepared frame; the later error text was `stream draw order
or state`. This is a failed runtime test, not acceptance of the prepared route.

Final runtime fault 45 means the prepared frame could not drain at shutdown.
All 357 private GPU signals completed and their recordings were released;
all 358 jitter changes and full render scale were restored. One stream-route
recording remained pinned, and hook cleanup did not complete. An independent
read of all 38 gated functions and 18 native hook heads found 17 hook heads
still changed; only the resolution hook was restored. The normal clean-stop
verifier correctly rejected the capture. No live patch/unload was attempted.

The user reported a normal scene and exited normally. Keep the current DLL
installed as requested, but prepare the correction and a fresh evidence root
before another test.

Code inspection identifies an argument-lifetime bug: `native_seh` calls
`finish_resolve(f->call.gpr[3], ...)` after `mhw_binding_forward`. That assembly
wrapper intentionally saves the native return register values back into the
frame, including volatile R8. R8 held the packet on entry but is not guaranteed
to hold it on return. A changed R8 prevents clearing the thread-local resolve
scope, making the next observation fail with route fault 8. The stored error
text is later than that first fault, so it does not establish a separate
inherited-render-target-state failure.

The next correction must preserve the entry packet independently of the return
register frame, exercise the real forwarding wrapper with a clobbered R8, and
allow hook removal on a faulted incomplete route while retaining uncertain GPU
resources. It must not falsely mark incomplete work retired or clear the fault.

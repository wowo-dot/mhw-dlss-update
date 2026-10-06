# Remaining color-grading draw, v1.23.10

This extends the v1.23.9 three-frame recorder with read-only CPU evidence for
the pass between motion filtering and final screen presentation. There are no
new hooks, SDK evaluations, native shader replacements, or output mutations.
The half render scale, three input captures, one scoped jitter sample and
restoration/retirement path remain unchanged. Keep native AA/DLSS Off,
MotionBlur Off, DX12 and windowed 2560x1440 for this capture.

The existing TAA final-copy selection runs after AutoExposure has constructed
its commands and before the motion-filter Execute. At that point the recorder:

- Finds exactly one stage15/order0 AutoExposure unit with vtable RVA3538400.
- Requires its stage15/order134217713 endpoint to be at unit+238, with null
  execution unit and the observed flags [1,0,0].
- Records the endpoint's assigned input/output owners from unit+248/+250;
  output must match the final-screen source.
- Scans the existing bounded command table (fewer than 65,535 entries) for
  exactly one opcode0 draw at key0x7ffffff1.
- Copies only that quad's 16 floats and its bounded PS DXBC (at most262,144
  bytes, read twice with identity/length revalidation).
- Revalidates the quad and owners after the motion-filter callback returns;
  its input must be the observed motion-filter output.

All data is copied before native arenas can be reused. No extra COM resources
are acquired. Only three records can be published. Existing frame/epoch/bank,
view, thread, reentry, SEH forwarding and clean-removal gates still apply. The
endpoint is constructed before its GPU sort position: the CPU capture order
does not claim that it already ran on the GPU.

The existing observer target remains MotionBlur Execute RVA24ca1f0, Windows
x64 member-function ABI (RCX unit, RDX render context), caller RVA24f228b. Its
original forwarding and lifetime assumptions are unchanged. The additional
static gate covers the complete AutoExposure Execute RVA24d0780, 4,636 bytes,
SHA-256 `5853f4dd6561e66842edab799f7d87ea3042f7b873dd72ab848e612579ff3e7b`.
No hook is added there. Tested host version0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decrypted analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

The postfilter JSON is schema2/policy postfilter-construction-v2, adding a
colorgrade record and three `.colorgrade-N.ps.dxbc` files. The external reviewer
checks frame association, queue budget, endpoint structure, full owner chain,
geometry addresses and artifact identities. Historical schema1 is still
reviewed against its original contract.

Verification: twenty callback tests include missing/duplicate color-grade
draws, invalid endpoint ownership, changed owners/vertices, SEH, wrong thread,
reentry and stop while active. Eighty-seven affected native/GPU regression
tests passed, plus seven current reviewer/header tests, six historical reviewer
checks (one additional case skipped), ten combined input/handoff reviewer
checks, and re-review of the accepted v1.23.9 live capture. Project build is
warning-clean. The new observation has not yet run in-game.

Deploy only with the game closed through the preserved MO2 slot. The local
v1.23.10 deploy script checks the old DLL, freezes tested sources and backs up
the exact destination and output sidecar. To remove this test manually with
the game closed, use that script's Restore action; it verifies both installed
and backup hashes before replacing anything. Do not restore automatically:
the user wants the latest DLL left installed.

This capture should qualify the native color-grading shader/quad for full-size
output integration. It does not qualify their GPU constants/states or claim
that [the offline filter adapter](scaled-filter-adapter.md) is active in-game.

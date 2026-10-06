# Scaled projection contract — v1.23.4

Prepared 2026-09-27 after the accepted offscreen 1280x720 -> 2560x1440 SDK
replays. This probe establishes the actual scaled projection denominator and
its relationship to the active input rectangle. It does not evaluate NGX or
replace the displayed image with DLSS yet.

## Runtime contract

The v1.23.3 input/depth/scale recorder is reused, with the established scoped
projection recorder enabled independently of SDK initialization. Three adjacent
frames are selected: unchanged projection, one native-table jitter sample,
unchanged projection. The sample has absolute native X/Y at least 0.3125;
selection is bounded to 20 lookaheads. The exact projection callback records
the viewport denominator, jittered/unjittered matrices and their relationship.
Each receipt must match its TAA frame, epoch, view, context, bank and async mode.
The camera jitter pair is restored in the existing exception-safe exit path.

There are 22 hooks (one more than v1.23.3), one scoped jitter write/restoration,
three input sets and the same two-second capture window. Active scale is 0.5,
allocation/content scale stays 1, and the existing native scale setter restores
full resolution before cleanup. The native TAA stage temporarily runs to expose
its established inputs; native AA/DLSS saved settings stay Off. No output-routing
mutation is present. The missing-local-color proof and final depth-draw route
retain their existing qualification and lifetime checks.

The added hook is the previously tested projection helper at RVA 0x228f3b0,
2000 bytes, SHA
`df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373`.
Windows x64: RCX camera+0x50, RDX renderer+0x710, admitted caller return RVA
0x228e834. The existing assembly forwards arguments/return state and preserves
nonvolatile registers. Camera, constants and view are borrowed only during the
callback; copied numeric receipts are retained. No additional COM ownership is
introduced. All 33 explicit native code gates are checked before deployment.

Tested host 0.0.0.0 SHA
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
captured image SHA
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
No original binary/assets are modified on disk.

## Jitter mapping

SDK render-pixel jitter is calculated from the measured native basis:
`(native_x * render_width / (2 * projection_viewport_width),
 -native_y * render_height / (2 * projection_viewport_height))`.
The new pure helper accepts a coherent render-sized or allocation-sized viewport
and rejects mixed axes, unknown sizes, invalid geometry and nonfinite samples.
For native [0.375,-0.4375] at 1280x720 render size, a 2560x1440 projection basis
gives [0.09375,0.109375]; a 1280x720 basis gives [0.1875,0.21875]. These are test
examples, not a claim about the live basis. The reviewer derives the result from
the captured matrices/viewport and exact scale/TAA frame association.

## Output routing finding

New read-only static exports are under `local/scaled-handoff-20260927/`, with
both executable hashes checked by the export utility. TAA2 Execute's final
call returns to 0x23931b3 and calls 0x23917a0 (143 bytes). It passes the selected
temporal owner in R8 and an attachment in R9. That helper saves native context
state, copies context rectangle +0x5c..+0x68 into an argument, reads source
allocation dimensions from owner+0x88, and calls 0x23c9390, then restores state.

The complete 983-byte 0x23c9390 helper has a fast path when engine formats match
and the rectangle fits the source allocation. It queues a copy-region packet
through 0x23c9250 (313 bytes). The packet has opcode 13, destination/source owner
at +0x10/+0x18, destination [0,0] at +0x30/+0x34 and source rectangle at
+0x38..+0x44. Otherwise it uses the System_Copy graphics route. Initial unwind
prefix exports are retained but were not mistaken for complete helpers.

Thus the handoff can copy only the active rectangle: writing a full allocation
at the TAA dispatch is insufficient to establish full-output consumption. The
actual live branch, destination and later spatial upscale remain unobserved by
this version. Future integration needs a qualified full-output handoff and
recovery route; this run does not change copy packets or force their rectangles.

## Verification and test

Warning-clean /W4 /WX build; 48 native tests passed, covering scale, activation,
projection restoration/SEH, stale frame admission, both viewport bases, geometry
and the existing scaled GPU routes. Two additional retained WARP cases combine
the projection recorder with multiple depth writers and private input captures,
including missing-local-color admission. Both pass exact capture reviews with
clean GPU validation. Four scaled-projection reviewer tests cover measured
bases, matrix/restoration damage, mismatched frame/scale and header budgets.
The prior v1.23.3 live capture still passes the updated reviewer. Existing
copy-path Python checks also pass. The combined tests caught and corrected the
reviewer's old assumption that scene-depth capture never includes projection
stimulus; the failed test logs are preserved.

Use the same bare offline MO2 launch entry, native AA and DLSS Off, windowed
2560x1440. Load a save and keep the camera still. The agent verifies the exact
new PID/startup before arming. A brief freeze or resolution change is expected.
Wait for saved evidence and clean restoration before normal exit. The probe has
no visual DLSS toggle; do not change settings during this capture.

Versioned evidence/deployment root: `local/scaled-temporal-20260927/v1.23.4/`.
Deployment preserves the previous DLL and exact sidecar and writes only the
authorized MO2 test slot. Keep this DLL installed after testing as requested.
Optional explicit rollback, with game closed: `deploy.ps1 -Action Restore`.
The next launch is pending; this is not a live acceptance claim.

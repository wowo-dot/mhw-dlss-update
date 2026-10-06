# Independent AA selection and AA-Off recovery

2026-09-26: foundation implemented and tested outside the game; the
[AA-Off resource control](independent-aa-off-control.md) is complete. Engine
integration is pending. User explicitly chose exclusive AA, independent of the game's TAA
selection, and unfiltered recovery rather than silent native-TAA fallback.

## Implemented components

`src/aa_control/policy.hpp` defines Native / Off / DLAA selection with no game-option
input. Temporary missing inputs suspend reconstruction, disable requested jitter,
and require a history reset on resumption. Actual failure remains latched across
later good inputs until explicit selection/retry. A second frame begun without
completion also latches failure. DLSS configuration is rejected until implemented.
The controller requires serialization and a renderer boundary before projection.
It does not itself change game options, schedule resources, or restore projection.

`src/temporal_present/unfiltered.cpp` validates a same-device, full-resolution,
single-mip R11G11B10 copy before issuing commands. It copies current scene color
into the selected output without shaders/history and restores both resource
states. It changes no descriptor heaps, root arguments or pipelines. The caller
must prove states and pin both resources/list through GPU completion and Reset.
This function alone is not a complete game fallback: already-jittered raster
color cannot be unjittered by a copy, and invalid copy eligibility needs upstream
renderer handling before a replacement invocation is published.

The SDK wrapper now exposes a last-attempt receipt: commands issued, bindings
restored, output written and success. Owned GPU fixtures can force failure after
conversion, after real Evaluate with a simulated failing result, and after the
native handoff. Production callers always use `RecordFault::none`. These tests
do not manufacture a real vendor failure or establish device-loss recovery.

The old experimental replace-or-native-TAA route was never deployed. Its edits
are preserved under ignored `local/independent-aa-20260926/superseded-bypass/`.
Runtime/dllmain source returned to the frozen v1.18.4 implementation while this
new controller is developed. Do not arm that old path as independent AA.

## Offline verification

- Release C++ build with /W4 /WX. 24 native checks pass, including selection,
  1,000 successful history steps, 1,000 post-failure attempts without retry,
  transient suspension/resumption, and exclusive routing counts.
- Nine hardware GPU cases: AA Off from COPY_DEST, nonpixel SRV and combined SRV
  states; DLAA without a native pre-pass; missing-state rejection; three partial
  recording failures; and 600 continuous evaluations with two queues and 60
  output allocations. All 600 recording generations released, peak three live.
- Each GPU case checks all 3,686,400 final native output pixels and both downstream
  compute/graphics results, with binding equality and zero GPU validation warnings
  or errors. Success matches independently packed DLAA output. Off/recovery equals
  the archived raw color exactly. Exclusive cases record zero native resolves.
- Four earlier one-frame SDK regression cases also pass.
- Ten bounded scalar-inspection tests pass after the live control's packed-height
  correction; the rebuilt C++ input-contract check also passes. CPU metadata is explicitly not proof
  of GPU contents, freshness, resource ownership, or shader execution.

Fixtures use owned D3D12 objects and private archived inputs, not a running game
or a redistributed game shader. Their synthetic metadata/jitter does not prove
live frame alignment or visual quality. No FPS claim follows from them.

Evidence, identities, source snapshot and hardware results remain ignored under
`local/independent-aa-20260926/` (`checks-final`, `aa-control-map.json`).

## Engine map and completed live control

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded comparison image
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both files were hashed before deriving this map. Proprietary disassembly stays local.

The known x64 AA apply function (RVA 0x43dcb0) calls the choice setter at 0x2287a00
then walks category 22, copying the table's two enable bytes into matching layer
flags. The setter maps choices 0/1/2/3 to TAA/FXAA bytes 00/10/01/11 at table
+0xe9e4/+0xe9e5. TAA2 has vtable 0x351bda8, FXAA 0x351c1a8 (constructor 0x2393680).
This confirms separate renderer controls downstream of the user's choice; it does
not prove a safe mutation boundary or the resources available with AA Off.

Generic update/reset dispatch gates use flag bit 0, whereas general AA enable
uses bit 1. The TAA2 update callback may therefore remain reachable when AA is
disabled. That is a candidate boundary, not a live scheduling guarantee.
Current TAA execution also has reset, auxiliary motion and sharpening work;
removing just the main resolve is not proof of complete independent AA ownership.

The bare-profile AA-Off control found allocated scene color/depth/motion and TAA
ring owners, but null TAA input/output bindings. Six focused snapshots had stable
fields within a single frame. Special mode=1 and table TAA=0 also establish a
second early-return gate inside TAA Execute. GPU contents/freshness and lifetime
are not established by these external CPU reads. Adapter/swap-chain format remain
unknown. The texture owner dimensions have a 14-bit height plus two flag bits;
both readers are corrected, with original capture evidence preserved.

After the user-confirmed normal exit, the unchanged diagnostic DLL's original
mode was restored. AA stays Off, Windowed 2560x1440/DX12 and native DLSS Off;
profile/modlist unchanged. The original TAA configuration remains backed up.
No mod controller or new rendering DLL was installed. Next: establish the
serialized pre-projection decision and independent render preparation/scheduling
boundaries described in the result, with qualified unfiltered recovery.

# Native AA-Off resource control

2026-09-26: the loaded-save control is complete. Native DLSS and AA were Off,
Windowed 2560x1440, DX12, bare offline profile. PID 57764, creation FILETIME
134349127635642237. The user confirmed normal exit; the previous diagnostic mode
was restored at 19:21:43 +03. AA remains Off for continued integration work.
No new rendering DLL, host hooks, remote calls or process writes were used by
this control. The existing legacy DLL was unchanged and set to `disabled` during
observation; it may retain transparent API observations.

## Confirmed CPU observations

Eight bounded external snapshots found table TAA/FXAA bytes both zero and both
matched layers' general AA-enable bits clear. TAA2 flags were 0x11, FXAA 0x19.
TAA2's local jitter/reset/velocity/sharpen bytes were 0/1/1/0. Its input at +0x150
and destination at +0x158 were null in every sample. Its three ring owners and
scene color/depth/motion owners still had non-null backing/resource pointers.
The category-22 list contained TAA2 and FXAA, with no exact TAA1/TAA3 matches;
this is not a claim that those other classes are absent globally.

All eight broad snapshots had stable list links, anchors and individually
reread resource fields, but each crossed a frame boundary. They do not establish
same-frame GPU input association. Engine formats were 34 for color/rings, 38 for
motion and 16 for depth. Their `gpu_format` metadata was UINT_MAX, not a DXGI
format. GPU contents, freshness, states and ownership/lifetime remain unverified.
The external reader logged device/swap-chain pointers but could not safely make
COM calls: adapter and swap-chain format are explicitly unknown.

A focused eight-sample follow-up had six samples with both unchanged frame/epoch
and all reread fields stable. It found special mode at singleton +0x2cda equal to
1 and the TAA table byte equal to 0. The live-verified TAA Execute code therefore
has an additional early-return gate that would still reject execution if only
the layer's enable bit changed. Stored camera jitter was zero. Generic update
counters varied and wrapped; they do not establish that a specific callback or
shader executed. They must not be interpreted as monotonic frame counts.

## Packed dimension correction

The frozen reader reported scene color as 2560x34208. This was a metadata decoder
error: the raw owner+0x88 word was 0x85a00a00. Width occupies bits 0..15, height
bits 16..29, and bits 30..31 are flags. The decoded extent is **2560x1440**, with
flag value 2. The meaning of those flags is not fully established.

This packing is supported by the texture routines at RVAs 0x239d650 and
0x239da50, including the latter's adjacent unwind fragment. Both complete spans
were compared to live process code before exit. Their SHA-256 identities are:

- 0x239d650, 529 bytes:
  `300d5250630b1d1199b85d4587e885f6cbfb753524b642774a81aa63f030684c`.
- 0x239da50, 288 bytes:
  `d197d9f0c23d729a83b422547207904ebb966d94262649594b19b13300867958`.

The original evidence and frozen reader are preserved unchanged. The current
Python reader now uses schema 2 and retains the raw word/flags as well as the
corrected extent. The C++ owner reader masks the height consistently. Actual GPU
descriptions still require GetDesc and resource validation. Ten Python tests and
the rebuilt Release `/W4 /WX` C++ input-contract test pass, including all four
flag combinations, maximum extents, and rejection of zero height with flags set.
These corrections were not installed in the game during this control.

## Consequence for independent AA

Allocated resources survive with AA Off, but the current native-TAA request hook
does not have its normal input/output preparation. A layer-enable toggle alone
is insufficient. This result narrows the integration work; it does not yet prove
that valid depth/motion are produced every frame with the option Off.

The next implementation boundary must own three things together:

1. A serialized decision before projection: mod mode, availability, scoped jitter
   and native TAA/FXAA suppression, without changing the user's saved selection.
2. Preparation and scheduling of current-frame color/depth/motion plus the output
   handoff, independent of native TAA being selected. Preserve required engine
   bookkeeping while avoiding its AA resolve and optional AA sharpening passes.
3. Transient suspension and latched failure handling. Stop further jitter when
   disabled and keep unfiltered rendering valid. A raw copy cannot undo raster
   jitter or GPU commands already recorded; recovery eligibility must be proved
   before publishing work. Native mode explicitly restores ordinary engine AA.

Map and qualify those renderer boundaries next. Do not activate the old v1.18.4
prototype as independent AA, persistently force TAA On, or count an unchanged
repeat of this CPU control as proof of GPU frame alignment.

## Evidence and restoration

Ignored evidence root: `local/independent-aa-20260926/`.
`review-aa-off.json` verifies all 468 sealed prelaunch files, the eight broad and
eight focused observations, two live dimension-code spans, and post-exit state.
The 303-source prelaunch snapshot remains unchanged; corrected sources are
separately frozen under `source-dimension-fix/`. Original host version 0.0.0.0,
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded comparison SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both were checked before deriving offsets/code evidence.

The DLL slot remains `689bef7c...`, its mode is restored to `native-depth-probe`
(`e94ac2a9...`), and `.dll.output-dir` is absent. Graphics remain `da3f357f...`
(DLSS Off, AA Off, Windowed 2560x1440, DX12 On); profile and modlist hashes are
unchanged. Exact original TAA settings and mode backups remain in the evidence
root. No new experimental renderer is installed or armed.

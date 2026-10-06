# Prepared command rejection result: v1.25.26

2026-10-01, controlled wingdrake and Gallery test. This run identifies two
structural refusals; it does not validate a rendering fix. Native AA/DLSS were
Off, DX12 windowed output was 2560x1440, and the mod rendered at 1280x720.
The existing Stracker-enabled profile and four recorded configurations stayed
unchanged. Executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Installed DLL SHA-256:
`74239e455632d8d5fa4e18b04f58835275128728c01ce4bd0a2eafe56ab029fe`.

## Confirmed command contracts

Wingdrake travel visibly suspended DLSS and recovered afterward. The completed
queue at stage 15/order 4100 contained opcodes `11,0,11,0`: two state packets
and two draws sharing key `0x78001004`. Draw arguments were count/first `4/12`
and `4/0`. The exact refusal was branch 5, the planner's one-draw-per-key limit.
At the controlled checkpoint, 990 such refusals and two intermediate-color
refusals accounted for 992 new skips. This proves the count restriction was
encountered, not that both draws' shader/input/geometry contracts qualify.

The Gallery also suspended DLSS, with no runtime fault, then recovered on exit.
Its graph contains an intermediate color connection at order 26005, type RVA
`0x3526650`, between the motion output and grading input. Current admission
rejects that connection with graph reason 6/detail 4. The first retained sample
for this shape predates Gallery playback: one opcode-11 packet, six opcode-6
packets, and one opcode-0 draw. It must not be labelled a Gallery-specific
shader recipe. The external recipe reader accepted 0/16 attempts; no shader
bytes were captured. Resolving this pass still requires its actual shader,
resource and coordinate contract.

Both retained command samples were stable and readable. Final diagnostics
counted 3,877 command refusals: 1,672 multiple-draw and 2,205 intermediate-color
refusals, with two distinct shapes and no overflow. Repeated-shape counters
do not identify every triggering gameplay action.

## Closure and evidence

The test was stopped intentionally after recovery. Final telemetry:

- 7,632 DLSS evaluations, 3,881 skipped attempts, zero runtime faults.
- 17 suspension/recovery pairs; not waiting at the recovery checkpoint.
- All 11,513 GPU signals completed; all 11,513 private recordings released.
- All 16,834 jitter injections restored; full resolution restored.
- Native code verified while the process was still open: 38 complete function
  spans and 18 hook heads restored. All hooks disabled.
- User confirmed a normal-looking scene and normal exit. Game and collector
  process absence verified afterward; DLL, sidecar and all four configuration
  hashes unchanged. Keep v1.25.26 installed as requested.

Evidence: `local/command-rejections-20261001`, PID 33364, creation
134352891968308322. Frozen source and tooling: `source-1.25.26`.
The collector completed normally with 75 events and 10,935,942 bytes; its
timeline SHA-256 is
`16a8ca6af61d587d2bb44a64c59c5585019e9eff784505f507794ab6c8e25bad`.
The full review accepted lifecycle/telemetry, not pixel quality or resolution
of the reported visual defects. The prelaunch evidence includes 56 focused
native tests, 44 Python tests and 288 GPU fixture frames. An earlier broad
CTest invocation was interrupted; it is not a completed full-suite result.

## Next work

Support a bounded sequence of distinct overlay draw packets under a shared
descriptor key while preserving native order and qualifying every draw.
Retain shader/input/geometry checks, conditional output selection, recording
order checks and GPU lifetime ownership. Establish the native equal-key order
before relying on queue insertion order. Duplicate packet identities must
remain invalid. Treat the Gallery color pass and the untraced hunter silhouette
as separate unresolved contracts; no effect-name exceptions.

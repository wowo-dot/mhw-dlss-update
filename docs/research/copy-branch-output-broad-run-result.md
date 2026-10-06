# Native copy branches, cutscene rectangle, and quest-abandon output fault

2026-09-30, v1.25.20. Evidence: `local/copy-branch-output-20260930`.
Follow-up to [native-copy-branch-admission.md](native-copy-branch-admission.md).

The broad run produced 15,314 successful DLSS evaluations before an actual
output-route fault. The user reported clean rendering in the smithy and rain,
and a rectangle during the cutscene/monster effect. Repeating the earlier
shutdown sequence reproduced the fault: the user abandoned the quest, and its
animation and menus appeared. No assistant or collector stop was requested.
The user subsequently confirmed a normal scene and normal game exit.

The same Stracker-enabled `barebonesdlss` profile was verified, with native
AA/DLSS/MotionBlur Off, DX12, 2560x1440 window/output and 1280x720 render extent.
Module paths exposed by MO2 were distinguished from backing mod files when
recording their hashes. Additional gameplay-mod activation was not independently
verified. The current DLL and configuration are preserved.

## Depth branch correction

The native opcode-10 branch reader admitted 15,425 fresh depth copies. There
were two schedule rejections (reason 5), but **zero invalid-child/node
rejections (reason 6)**. The former sustained smithy/rain failure did not recur
in this run. Direct region-copy and child-draw paths are represented in the
receipts. This supports the structural branch correction; it does not establish
coverage of every scene or all possible native copy branches.

## Cutscene rectangle

Two supplied 2560x1440 screenshots show darkening confined to the top-left
1280x720 region. Their filenames indicate 09:42:45 and 09:42:51 local time;
they are not matched to exact telemetry frames. The live shader capture at
09:43:09 was taken following the user's cutscene report.

Bounded reads saved seven pixel-shader recipes and seven associated vertex
shader recipes, with nine unique shader binaries in total. Descriptor chains
and shader bytes were read twice, validated as SM5 DXBC, and remained stable.
These are private local artifacts, not redistributable source assets.

The native draw at type RVA `0x3526650`, stage 15/order 26005, used a
1280x720 rectangle against a 2560x1440 color target. Its actual pixel shader
declares `CBVignetting` and `CBScreen`; the shader computes vignette darkening.
SHA-256: `4ec2bac0c6cca1a8206515e0792bef59b5380a73c8f3f649dfa85c0bff9f4b29`.
This is a strong candidate for the visible quarter-screen rectangle. Exact
visual attribution still requires a corrected rendering comparison.

The adjacent stage-15/order-1024 draw at type RVA `0x3536c98` also retained a
1280x720 rectangle on a 2560x1440 target. Its captured shader declares
`CBGodRaysFilter`, `CBGodRaysConfiguration`, `tBaseMap`, and `tOcclusionMap`.
SHA-256: `d28341d7ffb11473fab7cd7d87ba87be88e81a9a49eef61cec01599667343444`.
This identifies that captured shader from its contents; the earlier nearby
static string alone had not proved an effect identity. Other intermediate
draws legitimately used smaller targets/rectangles. Do not globally enlarge
every draw, or choose behavior from effect names. The fix needs consistent
target extent, geometry, viewport/scissor and sampling across the post-resolve
path, while preserving deliberately reduced intermediate work.

## Quest-abandon shutdown

The new output guard captured mask **2**, meaning a recording existed but its
final-output resource state was unknown. The destination resource, RTV,
pipeline and root signature all matched; raster bindings were available.
No incorrect known GPU state or SDK failure was established by this receipt.

The receipt identifies frame/epoch 32246, output stage 2 (final draw), command
list generation 220910, tick 55346328. The collector first observed stream
fault 6 at 09:48:12.562 local and terminal runtime fault 45 at 09:48:14.667.
The preceding healthy snapshot at 09:48:07.365 had seven total skips; the next
snapshot with the stream fault had 113. Do not treat the latter increase as
106 independently diagnosed normal-gameplay skips.

Static inspection shows `stream_route.cpp::barriers` only associates barriers
with a plan while selected copy, effect, or resolve callbacks set a thread-local
slot. State is stored per command-list generation. Missing observations outside
those scopes or inherited state across recordings are the next candidates;
this trace alone does not distinguish them. Do not remove the state guard or
equate an RTV binding with proof of a valid D3D12 resource state.

## Review and cleanup

The frozen full reviewer correctly rejects `clean stop`; this remains a failed
run. Independent depth, prepared-queue and output-fault receipt checks pass
17 native status records and 109 collector snapshots. The startup check passed.

All 15,427 private GPU signals completed. All 15,427 private recording records
were released. All 15,886 jitter changes and full render scale were restored.
Independent live reads verified 38 complete native function spans and 18 hook
heads restored. Two downstream recording records remained pinned until process
exit, as recorded by the fault path. Do not claim complete in-process resource
release. The game then exited normally.

The periodic status sidecar is stale after termination. Use the last continuous
record in the main log (also saved as `session-53880/terminal-status.json`) for
cleanup, not its older `active: true` sidecar snapshot. The collector ended on
the native terminal event after about 9 minutes 11 seconds, below both limits.

Next work: correct output-state observation/lifetime for the quest-return path
and make downstream effect sizing consistent with the reconstructed output.
Keep both changes structural. Preserve the depth branch correction and its
checks. No corrected follow-up build is prepared by this result note.

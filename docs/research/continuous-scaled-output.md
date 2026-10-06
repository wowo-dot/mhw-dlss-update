# Continuous Performance DLSS prototype, v1.25.0

Prepared 2026-09-29 after the accepted v1.24.5 one-frame live output. This is
the continuous implementation of that output route, using the established
native temporal scheduling, jitter, history, submission and retirement code.
Its continuous game behavior has not yet been verified.

## Rendering contract

The first test uses DX12, a 2560x1440 window, native AA/DLSS/MotionBlur Off,
and fixed 1280x720 rendering. The SDK uses Performance mode 0, preset K (11),
with full-size allocations, half-size active inputs and full-size output.
Motion and jitter are expressed in render pixels. Jitter conversion uses the
observed projection viewport basis rather than assuming it equals render size.
No game configuration writes are required to arm the feature.

The scaled target compiles the continuous DLAA runtime with
`MHW_CONTINUOUS_SCALED`; the original DLAA target retains its previous behavior.
Depth comes from the qualified native DrawInstanced depth-copy path, matched
to the TAA request's frame, epoch, view, bank, async identity and destination.
The existing request and GPU lifetime rules still apply.

After native postfilter queue preparation, a bounded CPU plan identifies the
temporal transfer copy, motion effect, color grading effect and final effect.
Admission checks their owner graph, queue keys, packets, resource descriptions,
vertex geometry, shader bytes and pipeline layouts. Three native warmup draws
prepare immutable private pipelines. A later matching frame can then replace
the native TAA resolve with DLSS, expand the transfer copy, and run all three
native effects over the full output. Native shaders are captured locally;
proprietary shader bytes are not embedded in the distributable source.

Each replacement restores the observed root, pipeline, vertex, raster and
output bindings. The copy override closes after the complete chain, preserving
later copies that happen to reuse its resource pair. There is at most one
claimed output chain at a time. CPU plan capacity is 32; resource retirement
capacity is 128 recording generations, with up to 24 deduplicated references
per generation. Capacity exhaustion never evicts live GPU references.

Every admitted list, output, native pipeline, root and vertex buffer remains
pinned until both its submission fence completes and its recording is reset.
The original continuous runtime serializes its private work across native
DIRECT queues. SDK history persists across qualified frames and uses the
existing reset policy. Per-frame CPU image readbacks are disabled.

Unqualified frames keep the existing unfiltered route without SDK admission.
SDK failure uses the established full-output spatial recovery and latches
reconstruction off. A route invariant failure requests shutdown. Shutdown
restores native scale while the render callback remains installed, drains
accepted work and disables hooks. Failure to prove drainage keeps references
pinned and records a fault; it does not pretend cleanup succeeded. This does
not change integrity, licensing or platform checks.

## Hook identity and ownership

All native locations are image-relative and accepted only for executable
version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The pinned decrypted research image SHA-256 is
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Function byte gates and structural checks precede hook activation. Absolute
machine paths occur only in local deployment configuration.

The shared temporal runtime hooks RVAs 0x23c9980, 0x259cc20, 0x23cd170,
0x259df30 and 0x228f3b0 using its existing register-preserving x64 assembly
adapters. Its COM hooks use the Windows x64 ABI and exact D3D12 signatures:
Reset, Close, ExecuteCommandLists, ResourceBarrier, Dispatch, CopyResource,
and the scaled-only DrawInstanced. The parent owns Reset generations, queue
submission, fences and depth/resolve resource lifetimes.

The stream route adds register-preserving native adapters at 0x24ca1f0
(completed postfilter preparation, caller 0x24f228b) and 0x259d670 (native draw
wrapper). Its CopyTextureRegion COM hook admits only caller 0x259caf0 with
the exact completed-plan packet and recording. The parent DrawInstanced hook
calls the route at caller 0x259d72e. The stream route owns its immutable recipes
and resource pins, and receives Reset/submission/state notifications from the
parent. It does not install competing hooks for those parent methods.

Native AA activation retains its eight gates at 0x228e770, 0x2392150,
0x23931e0, 0x2596090, 0x25979b0, 0x239217d, 0x2392f8e and 0x24f1fc0;
see `native-aa-postqueue.md` and the activation assembly for leaf-gate ABI.
Render sizing uses 0x229ed30 and shares viewport observations with the existing
23-method binding observer. The sizing byte gates are checked before AA
activation patches their callees. Total gameplay hook count is 47, plus the
separate startup capability hook, which completes before gameplay arming.

## Offline evidence and limits

Evidence root: `local/continuous-dlss-20260929` (ignored).

- Release builds passed `/W4 /WX`; 152 relevant native CTest cases passed.
- 206 Python review tests passed. An explicitly synthetic v1.25.0 complete
  log passed the combined reviewer, which rejected a missing output draw.
- The lifetime test exercised 22,000 resource changes and bounded exhaustion.
- WARP and NVIDIA route fixtures each passed 96 full output chains, including
  changing vertex buffers, 288 effect draws and 786,432 exact copied pixels.
  The actual CPU plan builder passed a valid synthetic native graph and nine
  damaged graph cases; six route rejection modes also passed.
- The real NVIDIA SDK completed 96 Performance evaluations, one initial
  history reset, ten native output allocations, three in-flight recordings,
  and alternating queues. All 96 recordings retired. GPU validation reported
  no warnings or errors. All 3,686,400 final packed pixels matched independently
  packed SDK output. The native-effect comparison checked 11,059,200 pixels
  exactly against its reference.

These fixtures use captured inputs and synthetic native state/constants.
They establish GPU routing and ownership, not temporal image quality or live
queue timing. The v1.24.5 live evidence established one output chain; continuous
area transitions, sauna, focus changes and sustained play remain live tests.
Resolution changes and quality-mode selection remain outside this first
fixed-Performance run. Reflex and frame generation are later work.

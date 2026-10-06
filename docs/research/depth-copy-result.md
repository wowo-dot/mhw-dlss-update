# Partial native depth copy observation — run 47748

Follow-up: [v0.7.1 run 6868](depth-copy-submission-result.md) completes the
corrected capture and submission association. This v0.7.0 run remains rejected.

v0.7.0, 2026-09-24. The overall capture is **rejected**. The recorder stopped
during the 50 ms provenance warmup, before the `armed` acknowledgment or selected
draw. No submitted copy generation, GPU completion, pixel contents or jitter
result is established by this run.

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256 `066548DC3713F43E43C50DCA8B4F9D99217130F443FB08418A13B6F8AFB4398E`.
Archive: `local/projection-binding-20260923/v0.7.0/`; `inspection-47748.json`
and `archive-47748.json` seal the raw channels, failure analysis and restoration.

## Partial observations

The six startup gates passed. The native copy wrapper at RVA `0x259DF30`
actually issued two `CopyResource` calls returning at `0x259E100`, on command
list 356236800. This resolves the earlier uncertainty about whether this
wrapper took its copy or fallback draw branch for the captured depth resource.

Source resource 356507280, canonical identity 356507232, is a 2560×1440
R32_TYPELESS Texture2D, one mip/array slice/sample, ALLOW_DEPTH_STENCIL.
The DSV watch independently recorded D32_FLOAT views of this same identity.

| Entry / return | Destination | Canonical identity | Description |
| --- | --- | --- | --- |
| 239 / 288 | 5234466608 | 5234466560 | 2560×1440 R32_FLOAT, one mip/slice/sample, ALLOW_RENDER_TARGET |
| 289 / 320 | 5234469056 | 5234469008 | Same description |

Immediately before both copies, barrier call 235/238 on that command list
declares source `0x10 → 0x840` (DEPTH_WRITE to COPY_SOURCE plus nonpixel shader
read), first destination `0x80 → 0x400`, and second destination `0x40 → 0x400`
(COPY_DEST). A different list records `0x400 → 0x40` for the second destination
at 306/309. These are recording observations; CPU order cannot prove their
execution order across lists. Reset/Close/submission association remains missing.

The result identifies two real copy destinations worth following. It does not
prove that either holds final-scene depth at a suitable temporal reconstruction
point, or that the earlier selected projection draw wrote these pixels.

## Recorder failure

There are five copy calls. A separate color-copy pair uses source 1844224256
and destination 1349006336 on two different recording threads/lists. The first
call spans 315/344; the second spans 337/340 and is the only invalid history call.
Both resources eventually publish ready=1, with first order 315, valid descriptions
and the correct device. v0.7.0 deliberately rejects a shared resource record if
another thread reaches it before initial COM qualification has finished.

These observations are consistent with that first-use overlap guard. The trace
does not distinguish whether the source or destination was still initializing.
The primary error is 2 (`read`); `provenance-prefix` is the worker's later report
that warmup failed. It is not a driver/device failure report.

All 55 view events, 117 history calls, 271 barriers, five copies and seven copy
resource records are saved. Hooks disabled, pending calls and retained references
zero, and capture drain succeeded. The user exited normally. The exact v0.15
DLL was restored, the output sidecar removed, and all four configuration hashes
are unchanged. All 58 archived v0.7.0 sources verify.

## Correction for the next run

v0.7.1 gives each observed copy two independent retained resource records, then
merges their metadata after all admitted calls have drained. It adds no wait or
lock between rendering threads. Each repeated observation must agree on identity,
device and every description field; mismatches still reject the capture. Merge
failure and capacity exhaustion still release all successfully retained references.

The physical reference bound is 1,024 (two per copy); the serialized unique-resource
bound stays 128. A deterministic fixture pauses the first query, lets another
list finish tracking the same resource, then checks that cleanup waits for the
paused call. Exception and conflicting-description/device/identity cases exercise
the same cleanup path. The next game test retains DLSS Off / TAA On and the same
bare profile, with the purpose of capturing a complete copy/submission interval.

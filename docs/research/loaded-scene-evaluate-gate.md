# Loaded-scene attempts without Evaluate

Date: 2026-09-07. Two new v0.7.0 logs, **37452** and **28340**, again validate
successful Create and copied-handle Release before both provider Shutdowns.
Both report **zero Evaluate calls and zero suppressed Evaluate calls**.
The [first copied-handle lifetime](first-handle-lifetime.md) remains accepted;
frame-input capture remains unresolved.

## User correction and runtime evidence

The user reports loading a save, returning to the main menu, enabling DLSS,
then loading the save again. **DLSS cannot be changed from the in-game menu.**
The earlier suggested in-game toggle procedure was not usable and must not be
repeated. The action sequence was reported for this new batch; its exact mapping
to the two process IDs was not separately supplied.

| Run | Log bytes | Create / Release / Shutdown sequences | Handle retention | Successful sizing events | Trace duration |
| --- | ---: | --- | ---: | ---: | ---: |
| 37452 | 64,756 | 68 / 75 / 76, 77 | 29,531 ms | 16 | 95,219 ms |
| 28340 | 63,673 | 72 / 74 / 75, 76 | 30,891 ms | 15 | 109,907 ms |

Archived, hash-verified local logs:

- `artifacts/logs/bridge-37452.run002.v0.7.0.log`, SHA-256
  `709F849DE4E45F424E8219DDF70368C729A949AD86A659B215BCAD2A451A2E65`.
- `artifacts/logs/bridge-28340.run003.v0.7.0.log`, SHA-256
  `79D6204BD288028938AC8E603B9A5AC8695772DD62D13399EE6BFA5A1ACB1F0A`.

Both start in recognized `capture-one-evaluate` mode. The DLSS Create returns
`0x1` with token `0x000000010000B12D`, from exact core caller `+0x67DE6`, with a
verified D3D12 command list. Each completes the same 11-query scalar contract:
2560x1440 input/output, node masks 1, quality 0, RTX 0, Hint.HDR 1; the two
ResourceWidth/Height keys remain absent. No resource pointers were captured.

Each Release at core `+0x8AB7` returns success with all four copied-handle checks
true, followed by successful DLSS and ISP Shutdown. Both summaries are inert
with no outstanding Init/token cleanup, no handle-read fault, no boundary
contention, no event drops or lifecycle overflow, and complete startup routes.
Each trace closes quiesced. No matching new MHW crash dump was found, and the
game was closed at inspection.

The 16 successful sizing events in run 37452 fill the event capacity. Its only
checkpoint is early, so absence of a reported exhaustion flag does not prove
that no later unlogged over-budget invocation occurred. Run 28340 records 15
successful sizing events, below the limit, and still has zero Evaluate calls.
The observations do not support solving this solely by increasing that budget.

The settings file now has **DLSS On**, DX12 On, full screen 2560x1440. It was
read but not modified during this review. The deployed diagnostic remains
v0.7.0; this analysis does not change the renderer or install another binary.
Structured local assessments and assertions are under
`local/evaluate-20260907/runs/{37452,28340}/reviewed-run.json` and
`local/evaluate-20260907/review_scene_runs.py`.

## Upstream investigation

Repeating the same scene sequence is not the next diagnostic. The provider
wrapper counted no Evaluate invocation across a complete lifetime, so inspect
the host's scheduling conditions before altering the provider capture code.
Equal input/output dimensions are a hypothesis to check, not an established
cause from these logs.

The captured host's reflection registration at `+0x2294000` maps `mUseDLSS` to
renderer byte offset `+0xA5`, `mUseDLISP` to `+0xA6`, quality to `+0xA8` and user
sharpness to `+0xAC`. A bounded scan of renderer methods and functions referencing
the known renderer global identified candidate DLSS-flag tests at functions
`+0x2391F30`, `+0x2392150` and `+0x2393260`. The scan itself proves only instruction
references; their control flow and live values require separate inspection.

These are read-only IDA analyses of the same captured-host SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`,
with database input MD5 independently checked. Bounded exports and discovery
reports are retained locally under `local/evaluate-20260907/`; no host bytes or
runtime flags are patched.

## Confirmed render producer and remaining unknown

Further bounded exports in `host-dlss-schedule/` and `host-temporal-layer/`
identify the producer of the type-3 item consumed by `+0x259DAE0`:

1. Temporal-AA2 execution at `+0x2392150` has early exits for missing/excluded
   render contexts and a separate context-mode condition. Its DLSS branch
   requires renderer `mUseDLSS` at +165, bit 0 of the sizing flags at +180,
   renderer backend readiness at `[renderer+120]+272152`, and a nonnull NGX
   wrapper at `[renderer+120]+272128`.
2. It passes that wrapper to `+0x23913F0`. When the wrapper is nonnull, this
   helper schedules `TemporalAA2_ResolveDenseVelocity`, assembles the borrowed
   color/motion/output/depth references, reset-related value and render rectangle
   into a 64-byte parameter record, then queues a separate command with opcode
   23, subtype 3 at +16, and the parameter-record pointer at +32.
3. The previously inspected D3D12 command consumer `+0x259DAE0` dispatches that
   subtype to `+0x2383CF0`, which sets the NGX parameters and calls Evaluate.

The inspected selector and enqueue helper contain no direct input/output
dimension-equality rejection. That narrows the 1:1 hypothesis; it does not
exclude an upstream layer-activation condition. Successful provider Create
alone does not prove that Temporal-AA2 executed or that all four live renderer
gates were true. The Create consumer's readiness assignment occurs after the
provider returns, so a sample taken inside Create would be premature evidence
for the final readiness state.

The current configuration also has `Anti-Aliasing=Off`. Its effect on this
layer's activation has not been established; do not label it the cause or
change it speculatively. The next probe should observe the specific renderer
flags and readiness after Create has returned, and whether the temporal pass
or its enqueue path executes. Keep the existing 1:1 metadata while gathering
that evidence. No additional runtime attempt is requested for v0.7.0.

The next implementation, [v0.7.1](renderer-gate-observation.md), begins with
read-only asynchronous gate and frame-index samples. Temporal-pass execution
and enqueueing remain uninstrumented. See its contract and current runtime
procedure before the next test.

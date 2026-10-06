# Depth-view lifetime and list-history recorder v0.4.0

Implemented 2026-09-24 from the [design](depth-view-state-plan.md). This is a
read-only diagnostic before GPU readback or a temporal implementation. Consult
`local/projection-binding-20260923/v0.4.0/preparation.json`, `deployment.json`,
`prelaunch.json` and any subsequent `restored.json` for the exact tested and
installed state. [Run 1544](../research/depth-view-state-result.md) accepts the
actual view provenance; local state and GPU proof remain incomplete. Its
runtime cycle is closed/restored.

## Observation and ownership

The tested executable remains version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
After the existing early trampoline reservation, the worker waits up to 120
seconds for the actual DX12 device and depth descriptor heap. Five complete
code spans gate the view constructor/destructor, handle allocator/recycler
and backend initialization. The later draw recorder retains its 39 gates;
the union contains 43 distinct spans.

The heap is `backend+188`, its CPU start is `backend+1A8`, and its stride is
`backend+1DC`. The complete backend-init gate covers the four-heap loop at
`23A5698..23A5722`: four descriptor types in order, eight-byte heap slots
starting at `+170`, with CPU handles stored at slot `+20`. Thirteen checked
instructions and the static four-descriptor table establish these fields;
the actual heap's `GetDesc`, CPU start, owning device and device stride must
agree. Device/heap references remain pinned until callback drain. Device,
heap, start, stride and backend anchors are rechecked during admitted calls.

The extra hooks are:

| Target | ABI and captured evidence | Lifetime |
| --- | --- | --- |
| Native `24E6400` | Windows x64: RCX new view, RDX backing, R8 twenty-byte recipe; returned view and stored handle/resource/readonly byte | Constructor entry through return |
| Native `24E8E40` | RCX view, remaining destructor arguments forwarded unchanged; retire matching references before original destructor | View generation |
| Device slot 21 | `CreateDepthStencilView`, including the actual 24-byte descriptor and destination | Nested native creation or unqualified overwrite |
| Device slots 23/24 | `CopyDescriptors` / `CopyDescriptorsSimple`, bounded destination ranges; omitted sizes mean one | Any DSV destination write on the observed device |
| List slots 9/47/27 | `Close`, `ClearDepthStencilView`, `ExecuteBundle` | Short history window |
| Existing list slots 10/26 | Actual `Reset` / `ResourceBarrier`; reuse the existing hooks | Short history window |

Native hooks and all SDK calls preserve the original arguments, return
registers, LastError and exception propagation using one original call.
`CopyDescriptors` has seven explicit parameters plus `this`: four stack
arguments are required. `ClearDepthStencilView`'s depth is in XMM3; stack
UINTs are decoded at their declared 32-bit width. No locks span original
calls or COM queries. Independently reserved slots publish immutable event
records only on return. An expired reservation has an invalid slot index,
so it cannot accidentally publish a still-running parent constructor.

A creation nested at return RVA `24E6516` pins the exact resource interface
and records its canonical `IUnknown` and owning device. The pin is released
on the matching native view destructor or final drain. Canonical identity
can differ from the resource-interface address. The prior recorder's three
stronger resource-query phases remain intact at the selected draw.

## Bounds and evidence levels

Startup watching lasts at most 30 minutes with 2,048 view events and 256
retained resource references. The journal's fixed arrays occupy 6,750,208
bytes (about 6.4 MiB), excluding referenced SDK objects and the existing core
capture. Overflow, changed anchors, unreadable arguments, exceptional calls
or unsupported targets close admission and invalidate qualification.

After arming, earlier API history starts 50 ms before source selection, with
4,096 event slots, 65,536 barrier records and at most 1,024 barriers per call.
Its deadline remains two seconds from history start; the core's own two-second
deadline starts after the prefix. A selected draw normally ends both sooner.
The prefix improves coverage but never substitutes for the selected list's
actual successful Reset. No silent reselection occurs after missing evidence.

The selected OM binding and actual draw have shared-journal ordering markers.
The companion JSON is hashed in the primary log and read only by its exact
sibling filename. Replay requires the actual descriptor creation to finish
before binding, a matching constructor generation, and no overlapping or
intervening destruction, reuse, creation or destination copy through the
draw. Cached OM state without an observed bind remains partial. Initial
support is writable, single-sample D32_FLOAT over R32_TYPELESS, one selected
mip/array slice; other descriptors are retained but unqualified.

List-local state requires a successful actual Reset after the retained
resource lifetime began, ordered nonoverlapping calls, matching resource and
subresource transitions, continuity and resolved split pairs. Relevant
aliasing, bundles, failed Reset or a Close before the draw prevent acceptance.
A clear is corroboration, not a replacement for a missing transition. Events
on other lists are never merged by CPU order. Even a complete local
DEPTH_WRITE declaration does not establish GPU submission, execution state,
contents, cached PSO creation or raster jitter.

## Offline verification and runtime steps

The preparation receipt requires Release `/W4 /WX`, 101 native tests and 164
Python tests. Real owned SDK fixtures create depth resources/descriptors and
record list calls without queue submission; they check reference balance,
handle reuse/copies, changed anchors, deadlines, capacities, concurrency and
exception cleanup. The eight-argument forwarding and depth-float checks cover
the new calling conventions. Native recorder components are tested separately;
the combined reviewer test is an explicitly synthetic composition of owned
traces, not an end-to-end game capture. Accepted game reviews 31912, 46324 and
11804 must remain identical; rejected runs 42212 and 5208 must remain rejected.

1. Launch the existing MO2 **Monster Hunter: World - DLSS 1.2.110 test** entry
   with `barebonesdlss`, no enabled mods, Stracker off, DLSS Off and TAA On.
   Use offline/private gameplay. No display-mode change is part of this test.
2. Load a save and stay in the scene. The startup watcher installs automatically;
   it reports `provenance_ready` followed by `ready` with watcher hooks on and
   core hooks off. A view created before this readiness may remain unqualified.
3. After confirming process generation and readiness, use
   `tools/arm_projection_binding.py` with that log to request the short capture.
   Require the `armed` acknowledgment, companion, completion and clean stop.
4. Review both channels with `tools/review_projection_binding.py`. Preserve
   partial findings without promoting them. Do not repeat an identical run
   to seek missing startup creation without changing the observation strategy.
5. Exit normally with DLSS Off. Restore the exact previous slot with
   `local/projection-binding-20260923/v0.4.0/deploy.ps1 -Action Restore`.
   Verify prior DLL/sidecar and all four configuration hashes.

The machine-local deployment replaces only the authorized MO2 test slot
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll` and its output-directory
sidecar. The helper preserves the exact previous files. All builds, logs,
shaders and analysis remain under the ignored repository-local archive;
the game executable and original assets are untouched.

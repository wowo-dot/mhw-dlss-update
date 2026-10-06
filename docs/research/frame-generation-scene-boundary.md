# FG final scene / HUD boundary: CPU diagnostics

Prepared 2026-10-03. This probe extends the accepted v0.6.0 DLSS Performance /
Reflex On renderer with four requested one-frame CPU maps. The v0.6.2 partial
metadata map passed runtime review; see `frame-generation-scene-boundary-result.md`.
The v0.6.3 probe follows returned texture copies and saves bounded
rejection-entry examples. Its four-sample runtime review passed as a partial
copy map; see `frame-generation-scene-copy-result.md`. Presentation and UI
semantics remain unverified.
It does not load the Streamline FG plugin, tag FG inputs, add GPU
commands, copy pixels, change presentation pacing or claim a HUDless image.

## Existing boundary and the missing proof

The scaled route already qualifies a final-screen transfer after its native
post-processing chain. A prepared plan retains its final packet, source/target
engine resource identities and participating command-list generations. The FG
guide requires the final post-processed scene and matching UI separation at
presentation time. The earlier DLSS resolve output is not sufficient evidence
for that requirement.

The next map identifies the final-screen target, direct draws that use the same
native backing label, and their ordered submissions. A draw after the final
transfer is a composition candidate, not automatically HUD: its shader,
blending and depth facts remain metadata until pixel contents and semantics are
verified. No new native offsets or hooks are introduced.

## Capture contract

`SceneTrace` owns fixed CPU arrays: four frames, up to 512 events per frame.
The v0.6.3 extension follows at most 16 resource labels per capture and retains
two incomplete entry snapshots per rejection reason (16 reasons, at most 32).
It holds no COM references and never dereferences stored identities. The
existing route mutex serializes requests, preparation, callbacks, submissions,
Reset receipts and Present linkage. Saturation, unreadable native state,
unscoped game draws, duplicate submissions and premature Reset are explicit.
Capture errors do not authorize rendering work or bypass existing renderer
guards.

A worker request selects the next qualified final-screen API draw belonging
to an admitted immutable plan. Capture 1 is automatic after at least one complete Reflex frame;
captures 2-4 use exclusive, atomically published generation-checked files.
Only one request can be pending. A captured frame closes at qualified Reflex
PresentStart only after its exact final draw has returned, the plan's rendering
choice is sealed, and all retained recording generations have submission
receipts. It stores the plan's frame/epoch, each callback's native counters and
Present's counters separately; those counters are not equated. It observes
entry to Present, not presentation completion or a GPU fence.

The probe reuses these existing interfaces and ABI contracts:

- Native non-indexed/indexed packet executors: existing hash-qualified x64
  assembly forwarding wrappers, with executor/context/packet in the retained
  register frame. Exception forwarding and original returns are preserved.
- DrawInstanced / DrawIndexedInstanced: declared Windows x64 COM signatures.
  Only game-image callers in those executor scopes are observed. Injected
  replacement/vendor draws are excluded; this is not a full D3D12 draw inventory.
- ResourceBarrier, successful DIRECT-list Reset and ExecuteCommandLists:
  existing continuous-runtime callbacks. The successful native Execute receipt
  supplies queue, batch ordinal, list index and exact Reset generation. It is
  not a GPU completion fence.
- CopyTextureRegion: existing Windows x64 COM hook, destination location and
  XYZ, source location and optional box. v0.6.3 records only game-image callers
  involving a followed resource, with a known valid Reset generation. Guarded
  copies of the two typed locations and optional box must describe texture
  subresource-index locations and a nonempty box if present. Placed footprints
  are counted as unsupported. Original arguments/forwarding and the existing
  predicated temporal-copy override remain unchanged. Only a returned,
  unreplaced original copy adds its destination to the CPU-observed path.
- The existing Reflex Present observer provides the presentation linkage.

At each retained draw the observer's current generation, PSO/root and single
RTV handle must agree with the native executor cache. Bounded guarded reads
retain its native backing resource label, packet, caller, arguments, shader
handle, blend/depth facts and rotating-view discriminator. The rotating handle
table is not traversed. A qualified final transfer requires a non-rotating
view; unresolved/unsupported state makes the metadata scope partial. Engine
extent/format values are labeled as engine metadata, not new GetDesc evidence.

The metadata recorder never changes bindings, selector choice, native calls,
resource pins or existing retirement. It does not use QI/AddRef/GetDesc, retain
shader bytes, log arbitrary process memory, add a per-frame disk write or wait
for a recording worker. Existing renderer diagnostics remain enabled. Status
and final logs serialize the bounded map from the worker.

## Review and current limits

The current reviewer validates v0.6.3 scope before explicitly normalizing its wrapper
identity to reuse the unchanged v0.6.0 ownership/v0.5.1 combined renderer checks.
It verifies capture requests, qualified final draw, current API/native binding
agreement, successful executor return, unique submitted recording generations,
bounded monotonic observations and the Reflex presentation owner. It derives
draw order from submission batch/list position and then within-list callback
order; CPU callback timestamps alone cannot order different lists. It declines
a cross-queue draw-order claim without producer fences.

Unmatched offscreen draws and unreadable/outside-scope calls stay counted. A
valid partial map can support investigation, but cannot prove every HUD draw
was found. An interface match with a sampled GetBuffer return is reported only
as an observed identity match; sampled historic interfaces do not prove the
currently indexed buffer or its lifetime. Indirect draws, bundles, additional
viewports, format conversions, texture reuse, immutable FG inputs and transparent
UI alpha remain unproven.

## v0.6.3 copy and rejection scope

The first label is the admitted final-screen target. Returned native texture
copies may extend that path; later draws and barriers may match an extended
label. Pointer equality and CPU callback order alone do not establish GPU
causality. The reviewer reconstructs a separate post-final copy path using
submission batch/list position, and declines that order claim across queues.
It reports any match with sampled GetBuffer interfaces as a historical identity
match, not current-buffer or lifetime proof. Copy region bounds, format
compatibility, complete contents and subresource coverage remain unverified.

Draw rejection reasons are: 0 absent executor scope; 1 executor-state read;
2 cached command-list mismatch/read; 3 packet mismatch/read; 4 active attachment
mismatch/read; 5 unsupported attachment mode; 6 backing-label reads;
7 rotating discriminator read; 8 unknown/invalid API generation; 9 unknown
single RTV; 10 native/API RTV mismatch/read; 11 root mismatch/read; 12 PSO
mismatch/read. Copy reasons 13 and 14 identify unsupported/unreadable copy
locations or boxes, and unknown/invalid API recording respectively. Reason 15
is reserved. Resource-path saturation has its own explicit counter.

Each rejection example records API list/generation, RTV/root/PSO labels,
caller and native clock, with the already-read native cache values. The cache
array is state/list/packet/attachment/active-attachment/flags/view/backend/
native-RTV/root/PSO. Zero is absent or unread, not a valid identity. The API RTV
handle does not prove which resource was drawn. These are entry snapshots;
they do not prove a successful return and never gate Present. Any later
submission receipt is optional and cannot upgrade them to qualified HUD draws.
No additional resource methods, memory blocks, hooks, references or GPU work
are introduced. Samples outside the two known executors can locate the missing
scope without guessing that they belong to the HUD.

Capture begins at the final draw's API entry. Concurrent command-list recording
may therefore miss earlier CPU callbacks that execute later on the GPU. The
bounded path is intentionally partial; a successful map does not establish
complete UI separation or authorize FG activation. Default target macros and
the accepted DLSS/Reflex rendering behavior are unchanged.

## v0.6.3 accepted scope and v0.6.4 diagnostic

All four samples linked a returned full-window native copy from the final target
to the same intermediate resource, ordered after the exact final draw on the
same queue. Neither resource matched the sampled GetBuffer interfaces. The
menu sample also contained four incoming 1280x720 copies; submission list
positions place all four before the final draw, including a CPU callback
observed later. This is a submitted copy path, not a HUDless/presentation image.

Reason 9 dominates the rejected matching-resource draws. Its current check
combines `raster_current()` with `output_known`: it cannot distinguish incomplete
IA/viewport/scissor/topology metadata from missing single-RTV metadata. The
existing OMSetRenderTargets observer only accepts one descriptor with no depth
descriptor pointer. Its forwarding wrapper passes the depth pointer as `c`;
the earlier range-flag hypothesis misidentified that argument. The run did not
retain OM arguments, so a depth attachment is not yet the established rejection
cause. Zeros in the later
native-cache fields are unread fields after an early return, not zero bindings.

The v0.6.4 extension retains successful native OM arguments and independent
raster-known flags through the existing observer, without relaxing rendering
admission. Its runtime validation is pending.
Then map the unresolved presentation attachment to the copied destination and
current indexed native buffer. Keep returned/submitted generation evidence
separate from resource format, pixel contents, ownership and producer-fence
proof before enabling FG.

The opt-in scene target links its own observer-library variant. Default targets
keep the existing observer compilation and rendering behavior. The diagnostic
variant retains the descriptor count, raw contiguous-range BOOL, presence and
guarded first-handle reads for RTV and DSV arguments, and independent binding/
raster validity flags. At most one RTV and one DSV descriptor are read after a
successful native OMSetRenderTargets call; descriptor labels are not resource
identities. Reset, ClearState or unsuccessful forwarding clears OM observations.
No additional API hooks, resource methods, references or GPU commands are added.

Each selected draw/rejected entry serializes a fixed 19-value raster observation:
entry-found, raster-hooks-installed, accepting, healthy, bindings-known, bindings-
valid, root-generation, raster-generation, raster-known-mask, vertex-mask,
OM-seen, count, raw-range-BOOL, RTV-argument-present, RTV-read, RTV-handle,
DSV-argument-present, DSV-read, DSV-handle. Raster mask bits are vertex=1,
viewport=2, scissor=4, topology=8, index=16, output=32. A separate 11-bit native
cache read mask distinguishes unread zero fields from successful reads. None
of these diagnostic snapshots authorize rendering or promote rejected entries
to completed draws. The v0.6.4 trace is schema 4 / policy
`submitted-scene-raster-scope-map-v4`; the probe log is schema 6. Current reviewer
acceptance is version-specific; accepted v0.6.3 source/reviewer remains frozen.
The v0.6.4 result is in `frame-generation-scene-raster-result.md`. The v0.6.5
depth-bearing draw and presented-buffer extension is described in
`frame-generation-scene-depth-presentation.md`.

The pure CPU tests cover bounded request/state rules, saturation, generation
retirement failures and reordered list submission. Reviewer tests reject
corrupted resources/returns/submissions, missing final linkage, truncation and
wrong scope; they also check that cross-queue order and UI semantics are not
claimed. The opt-in game target builds with C++20 /W4 /WX. All prior default
Reflex/DLSS target macros retain their existing behavior.

## Runtime procedure and identities

The v0.6.1 run is a rejected scene map, with healthy DLSS/Reflex rendering and
verified cleanup. Its automatic main-menu sample closed with no events and an
undecided choice. The gameplay sample closed with five events, including one
API/native matched final-target draw, but that draw's packet differed from the
selected plan's packet and the choice was still undecided. This does not prove
a scene/HUD boundary. Reading the code identified premature closure: it opened
at plan preparation and equated the producer's native counter with Present's
counter. The v0.6.2 correction opens at the exact final draw and closes only
after its returned/submitted receipt; all four runtime samples passed this
association check. No
renderer choice, native call, resource lifetime or presentation routing changed.

The first independent v0.6.1 cleanup reader failed because the deployment
receipt omitted its expected decoded-image hash key. The preserved v2 reader
uses the exact hash documented below, and completed all 37 hook-head and 46
full-function checks (778 reads / 75,163 bytes, zero writes). The original
reader and rejection remain in local evidence. This reader correction is
separate from the scene recorder correction.

Use the existing MO2 diagnostic slot with exact DLL/output-sidecar backups and
the game closed. Keep native AA/DLSS Off, DX12 On and the fixed windowed size.
Arm at the main menu; capture 1 supplies a control. After save loading, request
capture 2 while a visible gameplay HUD is present. Request capture 3 with the
menu open, then capture 4 after an area load. Stop intentionally, review both
renderer channels and all maps, independently verify native restoration, then
exit normally. Keep this DLL installed between tests as requested. Removal
instructions and exact source/DLL digests are in the local deployment receipt.

Retail version 0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded research-image SHA-256, rechecked before this work:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The existing 37 native hook heads / 46 full-function gates and ownership
assumptions remain unchanged. The FG chain's Off/retirement/shutdown lifecycle
is still separate work; the current Reflex-only SDK shutdown contract is used
only while the Streamline FG plugin remains unloaded.

Private evidence root: `local/frame-generation-scene-20261003`. Source, builds,
checks and eventual captures stay repository-local and ignored. Original game
files, assets, runtime sidecar, mode and saved graphics settings are preserved.

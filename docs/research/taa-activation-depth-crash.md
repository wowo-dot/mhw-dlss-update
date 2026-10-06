# TAA activation and the pre-Evaluate depth-resource crash

Date: 2026-09-22. Bridge v0.7.3, NVIDIA 610.88, DX12, full screen
2560x1440. Live inspection established an AA-dependent activation gate; the
next run reached DLSS work-item preparation and crashed before NGX Evaluate.
This is diagnostic progress, not accepted rendering or a captured Evaluate.

Follow-up: [run 14388 captured the actual failing work item](depth-exception-capture.md)
and confirmed the backing mismatch with 105 successful scalar reads. The
remaining inference in this earlier note is resolved for that later run.

## Identity and method

The executable SHA-256 is
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
file version `0.0.0.0`, PE timestamp `0x66D13CB6`, SizeOfImage `0x55D3000`.
All host addresses below are RVAs for this exact executable. Static analysis
uses the recorded captured host, SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`,
IDA input MD5 `A137D3636F22A730F71522C193B0472E`. See
[environment revalidation](driver-61088-revalidation.md) for vendor and Windows
identities and the unchanged v0.7.3 DLL hash.

The external reader opened MHW with query and VM_READ access only. It verified
the executable hash, mapped PE identity, five renderer code sequences and 15
temporal sequences. Additional early-exit and layer-list observations checked
the corresponding live instructions against the captured host. Reads were
bounded scalars with memory-region checks and pointer rechecks. No live writes,
new hooks, remote function calls or GPU work were introduced. Asynchronous
pointer stability does not establish ownership or frame coherence.

## Run 20564: a present but disabled temporal layer

Create succeeded at tick 4165312. Two renderer observations reported ready
selector gates, frames 42830 through 43277. The copied handle was released
cleanly at tick 4168406, **3,094 ms after Create**. Later sizing rejected the
consumed one-shot state. The user confirmed Alt-Tab during this interval;
whether focus change or loading caused Release remains unproved.

The worker's five-minute window ended while the game remained open. Its 57
valid temporal samples (507 scalar reads) retained four sentinel cache values.
All 13 sizing calls were accounted for: 12 successes, one invalid-state
rejection, no budget exhaustion. Shutdown was outside that logging window;
the final pending Init cleanup is not evidence of either a leak or accepted
Shutdown. External inspection continued after the worker stopped.

With saved AA Off, the loaded scene showed:

| Observation | AA Off | After selecting TAA through the game menu |
| --- | --- | --- |
| TAA2 object vtable | `0x351BDA8`, present | Same type, present |
| Layer flags at +20 | `0x11`; mask `0x2` clear | `0x13`; mask `0x2` set |
| Permit byte at `*[0x51C4368]+59876` | 0 | 1 |
| Execute's special-mode early exit | mode=1, permit=0 | mode=1, permit=1 |
| `CBScreen` cache at `0x4BE60D0` | `0xFFFFFFFF` | `0x9446D00C` |
| Deferred TAA2 record | Absent from sampled queue | Code 15, argument 2, owner TAA2 |

The actual layer list was traversed from `*[0x51238C8]+5584`, following +48
next links, with a 512-node bound. It completed at 25 nodes with a stable head
and one matching TAA2. Its three history pointers and dense-output pointer
were present even while disabled. Presence alone does not validate resources.

The queue changed from four entries to five. The TAA2 entry had override=0,
owner flags `0x13`, and its record was embedded at owner+320. A separate
40-sample comparison confirmed the game was foreground while mode=1 and
permit=0 persisted. Thus current background focus does not explain that
early-exit gate. This does not resolve the earlier Release cause.

Static AA application at `0x43DCB0` explains the observed change. The diagnostic
callback returns `DLSSMode=1` and 1:1 dimensions. Its consumer at `0x2383EF0`
produces feature flags=1, which leave the caller's AA choice unchanged. Flags=2
select AA choice 3; flags=3 select choice 1. The setter at `0x2287A00` maps
choices 0/1/2/3 to the word 0/1/256/257 at +59876. Consequently, the diagnostic
1:1 result with saved AA Off does not automatically enable TAA2. Normal menu
selection of TAA demonstrated the activation path without patching those flags.

Cache values remain historical lookup results, not per-frame execution counts.
The raw renderer `use_dlss` byte also remained 1 after Release; it must not be
equated with the menu setting or a live feature handle.

## Run 44972: depth preparation fails before Evaluate

The next process started with saved DLSS Off and TAA selected. The user enabled
DLSS once and reported a crash. Create succeeded at tick 4849453 with the
verified D3D12 command list and all 11 scalar queries completed. A renderer
snapshot at 4849718 reported ready gates and frame 5708. The later temporal
sample at 4853390 contained all four initialized caches:

- `CBScreen`: `0x9446D00C`.
- `CB_DL`: `0xC54FA1ED`.
- `TemporalAA2_ResolveDenseVelocity`: `0x10D4`.
- Dense helper `rwOutput`: `0x7E8D516B`.

There is no logged Evaluate, Release or Shutdown. The exception dump supplies
stronger evidence than the caches: thread 41292 faults at **`0x22927F2`** with
`C0000005`, reading address `0x70`. The fault bytes decode to
`mov r10,[rdx+70h]`, with RDX=0. This is the game's resource-state preparation
helper, before the bridge's Evaluate wrapper.

The helper has executed only its initial `push rbx`; RSP+8 holds the direct
return address **`0x229E89E`**. In the captured host this is the return from
the third preparation call for opcode 23, subtype 3, inside `0x229CEE0`:

1. Color owner at payload+8, backing at owner+96, state argument 4.
2. Motion owner at payload+16, backing at owner+96, state argument 4.
3. Depth owner at payload+32, backing at owner+240, state argument 4.
4. Output owner at payload+24, backing at owner+96, state argument `0x400`.

**Inference:** the depth owner's +240 backing supplied null to the third
call. The dump confirms the null argument and direct return, but does not
contain the heap payload, depth object or caller code page. Those missing
regions are reported as unreadable, not zero. The depth-field attribution
therefore combines the exception with the hash-recorded static call sequence;
it is not a direct heap-field observation or a complete stack unwind.

TAA's helper at `0x23913F0` constructs that 64-byte payload and queues its
64-byte descriptor. It gets depth through `0x255CE10`, which returns
`*(*(renderView+1576)+160)`. The downstream Evaluate preparation at `0x2383CF0`
would instead obtain the depth native object through owner+96, then backing+32.
Ordinary resource preparation first tests owner+96 and uses +240 as an
alternative when the owner kind at +88 is 6. The DLSS-specific preparation
uses +240 unconditionally. This difference motivates checking the actual
owner type and both layouts; it does not establish that substituting +96 is
correct, or that 1:1 sizing caused the mismatch.

## Run 8280: live depth representations with DLSS Off

The user reached the main menu with TAA selected and DLSS Off, then loaded a
save without enabling DLSS. Three samples at each checkpoint showed the same
depth-owner class and backing arrangement, with the game foreground, advancing
frames and stable rechecked chains. The depth getter and the DLSS preparation
call sequence were also verified byte-for-byte in this live process.

The owner vtable `0x351C870` identifies **`nDraw::Texture`**, a 280-byte object:
its metadata getter is `0x239D380`, and metadata registration at `0x212F30`
records that name and size. These are the game's custom type records, not
assumed MSVC RTTI. Both +96 and +240 belong to this same class.

The backing setup at `0x239C150` first creates/stores a resource at +96.
When owner kind +88 equals 6, it moves that pointer to +240 and clears +96.
Selected live code checks cover this move and the field reads. A null +240
on another kind therefore need not mean allocation failed.

The main view's +1576 container has vtable `0x35884E0`. Its property metadata
at `0x269E930` names member +144 **`mpRTDepth`**. The DLSS getter instead
returns member +160. In the loaded save, a three-sample pair comparison found:

| Member | Getter result (+160) | `mpRTDepth` (+144) |
| --- | --- | --- |
| Owner class | `nDraw::Texture` | `nDraw::Texture`, distinct object |
| Kind at +88 | 5 | 6 |
| Dimensions | 2560x1440 | 2560x1440 |
| Engine format ID at +144 | 16 | 17 |
| Engine GPU-format field at +148 | `0xFFFFFFFF` | `0xFFFFFFFF` |
| Texture flags at +112 | `0x600000` | `0x500000` |
| Primary backing (+96) | Present | Absent |
| Secondary backing (+240) | Absent | Present |

The getter result's primary native-resource pointer (+32) and state-tracking
pointer (+112) were present. No COM methods were invoked and no native D3D12
resource descriptions or contents were captured. Engine format numbers must
not be labeled as DXGI formats. The secondary owner's native resource, states
and image interpretation remain unvalidated.

Static container construction at `0x269D8F0` explicitly requests `RTDepth`
with kind 6 / format 17 / flags `0x500000`, and separately creates members
+160 and +168 with kind 5 / format 16 / flags `0x600000` when view flag mask
`0x4` is set. That mask is set in the sampled main-view flags `0x2D00D1FD`.
This supports a deliberate pair of representations, rather than assuming a
missing allocation. It does not identify the content transformation between
them or establish why the DLSS preparation code uses the other backing field.

The two kinds and slot arrangement are directly observed with DLSS Off.
Their presence in the exact run-44972 work item remains an inference: neither
that payload nor its owner was included in the crash dump. A future targeted
exception capture should read those specific fields at the fault before
choosing a correction. Simply substituting `mpRTDepth` is also inconsistent
with the downstream Evaluate setup, which expects a primary backing at +96.

## Recovery and next diagnostic

With MHW closed, the exact post-crash graphics file was backed up and only
saved `NVIDIA DLSS=On` was changed to `Off`. TAA, full screen, DX12 and
2560x1440 remain selected. The deployed v0.7.3 DLL is unchanged. Its existing
14-test build result does not validate this newly reached host resource path.

The attended **DLSS Off / TAA On** baseline was complete in the main menu and
loaded save. Keep saved DLSS Off. The next useful activation capture must
preserve the actual opcode-23/subtype-3 payload before process teardown:
wrapper, four resource-owner fields, their kinds and +96/+240 backing presence,
with bounded reads and the exact caller/code identity checked. In particular,
distinguish unreadable memory from absent pointers and compare the selected
depth with the view's +144/+160 objects. This can be captured at the existing
exception with an external diagnostic; do not add an unconditional host null
bypass or submit substitute resources to NGX.

No new activation capture or rendering fix was deployed during this initial review.
The later [external capture](depth-exception-capture.md) completed that next
step; no further crash is needed to establish the mismatch.
Future crash-producing tests remain useful when they resolve a specific
unknown; the user explicitly accepts that tradeoff. The bridge's provider
lifecycle checks alone cannot fail closed over this upstream host crash.

## Local evidence

Ignored directory: `local/driver-61088-20260922/`. It contains the bounded
reader, archived logs, scalar JSON captures, selected static exports and dump
report. The failed external run-44972 reader printed samples but did not save
its JSON before process exit; it was subsequently changed to retain partial
samples. The bridge log and dump independently establish the findings above.
Run 8280's four `live-8280-*.json` files preserve the main-menu and loaded-save
depth observations. The external script now saves partial completed samples
on reader failure. No native implementation changed in this review, so the
previous CTest result is recorded as previous validation, not a new runtime
acceptance. The new depth reader passed actual read-only captures in run 8280.
That run's archived log ends at its five-minute window with no Create or
Evaluate; later exit does not provide logged Shutdown acceptance. Final review
confirmed MHW was closed, the executable and active DLL hashes were unchanged,
and the graphics file differed from the post-crash backup only by DLSS On to
Off. `live-review-final.json` records those checks and artifact hashes.

| Artifact | SHA-256 |
| --- | --- |
| `bridge-20564.window-complete.log` | `9855D567B7D01F13AE4DF55442740499300D51E23E4A4C059BFA408FE1BBAE1A` |
| `bridge-44972.crash.log` | `1953A020DDF4492762ACE7D99C338AF707F21B5D3E5AD134AF95E2A700A02503` |
| `MonsterHunterWorld.exe.44972.dmp` (78,774,121 bytes) | `D9D778526171320834DA40CDD0CB20E5701CBD4E048C55E70F2C03AD4C3FA3C3` |

Proprietary exports, binaries, dump contents and IDA databases remain untracked.

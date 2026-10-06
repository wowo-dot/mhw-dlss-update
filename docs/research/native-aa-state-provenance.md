# Native scheduled state provenance — v1.21.2 diagnostic

Prepared 2026-09-27 after the [four state rejects](native-aa-state-trace-result.md).
The local state gate remains unchanged. This build records bounded native
schedule/ownership evidence at the existing rejected checks, without new hooks,
GPU commands, COM operations, settings writes or state-admission exceptions.

## Offline findings

Pinned host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both were verified before deriving new offsets. All disassembly stays local.
The initial unwind fragment at 23C9980 is preserved separately from its full
321-byte function; unwind entries alone do not define complete functions.

The native main planner at 229CEE0 processes the scheduled packets before
worker consumption. It resets counts at renderer +7AF28/+7AF50/+7AF68,
uses renderer +1F208 as an epoch and builds an array of 16-byte entries at
renderer +7AF18 (packet pointer at entry +8). It clears packet +8 before
building the linked resource-state list. Its ordinary compute path at
229E246 calls 2292610 through 229E273 using the shader's binding recipe.

2292610 selects SRV/UAV backing objects and calls 22927F0 with desired native
state words: compute SRV 4, UAV 400. 22927F0 reads the resource's tracker at
backing +70. It can reuse an existing compatible read state, merge read-state
bits, append a state-use span, or emit a packet state node. These are planner
records, not a D3D12 GetState operation. Later packets can change their values.

At 229EB22, when the schedule exceeds 100 entries, the planner examines
following packets' state lists. Subject to last-use checks, it links their
barriers onto an earlier packet and clears the later packet's list. Thus
`packet+8` at the TAA consumer does not necessarily contain every transition
that established TAA's input state. This is a confirmed static optimization;
v1.21.2 will test whether it accounts for the live missing-color samples.

Tracker layout derived from those writers:

| Offset | Field observed |
| --- | --- |
| +00 / +08 | First/latest planned use span |
| +10 | Epoch associated with those spans |
| +18 / +1A | Native state before that planning epoch / latest planned state |
| +1C / +20 | Last-use ordinal / its epoch |
| +28 | Initialization flag used on first-use paths |
| +2A / +30 | Separate immediate-path state / epoch |

Use span layout: packet +00, last-use ordinal DWORD +08, next +10, previous +18,
native before/after WORDs +20/+22, first-use flag byte +24. It is 40 bytes,
allocated from renderer +7AF40 with count +7AF50. State nodes are 32 bytes
allocated from +7AF58 with count +7AF68, and reference their use span at +10.
The arena counts reset during planning. No pointer is a persistent identity.

Texture backing constructor 24E7A10 allocates a 56-byte tracker, zeros it,
sets its initialization flag, then initializes its before/after words from
the native initial-state selection. The backing destructor 24E9240 calls
25538E0; at 255395B–255397B the tracker is freed and its backing pointer
cleared, before releasing the resource at backing +20. A resource COM pin
therefore does not establish ownership of its native tracker or use spans.
Do not retain and later dereference those native pointers as if pinned.

## Diagnostic contract

Existing hook ABI, ownership and removal rules remain unchanged. Four extra
live code-hash gates verify the newly interpreted scheduler/constructor layout
before installation; none of these functions is hooked or invoked by the mod:

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| 2292610 | 304 | `4adcc8e99a9ff16ba9af27c8db165b998958561d82da0d3c9e7bcdcb82cd37f9` |
| 22927F0 | 736 | `bef3f834e34a78dce1392d047062123d7c2d5633de7390aa0216cb654519e2ed` |
| 229CEE0 | 7756 | `11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3` |
| 24E7A10 | 2620 | `7ea61c6f4bbb21c5d8a25d5801a92476b825595c96937ac57d1e44870caea6fb` |

`state_trace` schema 2 retains the original first-eight-plus-latest rejection
policy and API/native-node records. Every sample adds a `provenance` object:

- Three trackers, each with at most 16 use-span copies. Header values are read
  again afterward and equality reported as `stable`; this is observational
  consistency, not atomicity or proof that the span contents cannot change.
- At most 32,768 scheduled entries searched in chunks of 64, keeping only the
  matched packet's ordinal and match count. Oversize, unreadable or ambiguous
  schedules do not acquire a valid position by assumption.
- Up to 32 earlier scheduled packets; at most 128 state nodes visited per
  packet and 16 matching color/output/motion nodes retained. Truncation and
  read failure are explicit; missing evidence is not synthesized.
- The schedule array/count/epoch are reread for a stability observation. Even
  equality does not prove GPU execution order across command lists or queues.

Bad reads do not fault the renderer or broaden admission. The retained sample
is constructed in place under the existing mutex, avoiding a large sample on
the game worker stack and per-capture heap allocations. No raw bulk memory is
logged: copied entries are only named state fields and relevant identities.
The arm/stop tool permits a bounded 16 MiB log because the richer snapshots can
exceed the former counters-only limit. Latest status remains bounded at 1 MiB.

The reviewer independently replays the existing API proofs and checks the new
serialized bounds, pointer linkage and packet-order association. It does not
claim a native intended state is an independently proven GPU state. A missing
or changing native record remains useful negative evidence.

## Verification and next run

Release build passed project /W4 /WX. 39 native and 128 Python checks passed,
including actual native owned-memory fixtures for an earlier transition,
future planned state differing from the current span, duplicate packets,
oversized schedules, pointer overflow, cycles, changing epochs and the bounded
predecessor window. Existing 4,096 state-gate parity cases still pass.
The old 1.21.1 capture is accepted unchanged by the new reviewers.

Next test: same bare MO2 entry, native AA/DLSS Off, Windowed 2560x1440, DX12;
arm after save load. Inspect baseline provenance and repeat one area load only
if it provides additional evidence. No rendering behavior has been repaired yet.
Keep CMFM/UI and resolution-changing DLSS work deferred.

Evidence preparation root:
`local/native-aa-state-provenance-20260927/v1.21.2/`.
The preceding DLL and its output sidecar are preserved before replacement.
Keep the diagnostic installed after testing unless the user requests otherwise.

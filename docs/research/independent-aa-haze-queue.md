# Optional haze changes the native post queue

2026-09-27. The [v0.8.1 capture](independent-aa-admission-result.md) identified
an ordinary native queue expansion, not a DLAA SDK failure or descriptor reuse
collision. The four-node-only plan was too restrictive for the observed scene.

## Confirmed identity and control flow

Before deriving this mapping, the host SHA-256 was checked as
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`, version
0.0.0.0, and the decoded comparison as
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Ignored evidence is under `local/independent-aa-queue-layout-20260927/`.

Vtable RVA 0x35372b0 has its phase callback at 0x24c7760. Adjacent type text is
`uHazeFilter`; that callback uses the `Haze` technique, `CBHazeFilter` constants,
volume noise and depth inputs. This identifies the extra pass as haze. The
nearby text is not presented as MSVC RTTI: this executable uses its own type
information, and the word before this vtable is not a complete-object locator.

The callback starts with a haze-scale check at unit+0x1bc. In the enabled
primary phase, while the manager is collecting, it calls the normal native
registration helper at 0x24c9270 and returns. During scheduler playback, the
collecting flag is clear and it records the filter using assigned endpoints.
The observed descriptor is unit+0x140, phase 15/order 20000/flags 0,0,0.

Native phase RVA 0x24f1fc0 first sorts the entire queue, then assigns every
admitted descriptor's input/output, then invokes callbacks. Thus a newly
registered haze descriptor can have null endpoints at collection, while the
other descriptors still contain assignments from a prior frame. The observed
new node did have null input/output and its four predecessors were unchanged.
Collection-time endpoint equality is not a valid final-graph invariant.

The scheduler's assignment code (0x24f2042-0x24f224b) and callback loop remain
under the existing 835-byte code gate:
`1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190`.
The 2,256-byte haze callback span at 0x24c7760 is additionally gated in v0.8.2:
`d1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701`.
It includes the contiguous callback body, whose unwind table splits the early
branch into a separate range; the initial 38-byte unwind entry is insufficient.

## Endpoint model

Let A/B be input/output view attachments and S0/S1 the two scratch owners.
The four owners must remain non-null and distinct. The full-size AA-Off
rectangle, descriptor identities, phases, orders, flags and vtables stay checked.
Only these two native layouts are admitted:

| Order | Base input -> output | With haze input -> output |
| --- | --- | --- |
| 0 | A -> S0 | A -> S0 |
| owned 2 | null -> S0 | null -> S0 |
| 100 | S0 -> S1 | S0 -> S1 |
| 1024 | null -> S1 | null -> S1 |
| haze 20000 | absent | S1 -> S0 |
| final 134217713 | S1 -> B | S0 -> B |

The owned in-place descriptor advances neither native ping-pong index nor the
native endpoints. Haze adds one swap; the final native copy therefore consumes
S0 while haze is present. When haze disappears, that copy returns to S1 even if
its collected descriptor still contains the previous S0 input.

The new plan saves both the collection snapshot and predicted assigned graph.
It does not dereference stale collection input/output fields. Before any DLAA
publication, the callback compares every assigned native endpoint with the
prediction. The native return and restored queue must agree as well. A mismatch
still latches off. Unknown pass sets and native TAA/FXAA remain rejected.

## Insertion and retirement

The insertion/removal transaction uses the validated native count N (4 or 5),
publishes N+1 with compare-exchange, preserves unused entry N, and restores
exactly N native descriptor pointers in their original enqueue order. Native
endpoint changes made by the scheduler are preserved; stale endpoint values
are never written back. Foreign descriptor changes still prevent blind removal.
No new game hook is added and no native haze invocation is replaced.

The plan, mutation and independent review tests cover 144 native enqueue
permutations, 720 six-entry removal permutations, exception cleanup, foreign
collisions, haze appearance/removal, stale endpoints and corrupted assigned
graphs. The normalized live failure matches the haze fixture. GPU submission
and lifetime tests also passed. These establish offline behavior; live haze
continuity and the two actual graph receipts are the next verification.

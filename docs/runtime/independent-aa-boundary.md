# Independent AA-Off DLAA input-boundary diagnostic, v0.8.3

The [v0.8.2 sauna run](../research/independent-aa-haze-result.md) passed the
native haze graph checks, then stopped at fault 19 before an SDK evaluation.
The exact resource-transition predicate was not recorded. This revision adds
bounded evidence for that predicate; it does not relax it or change the SDK
pass, source jitter, history policy, native haze handling or failure latch.

## Hook and recording contract

Hook targets, Windows x64 ABI, executable gates and lifetime rules are unchanged
from [v0.8.2](independent-aa-haze.md) and the
[continuous runtime](independent-aa-continuous.md). Tested host version 0.0.0.0,
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

For each tracked resource in an observed Reset generation, record the latest
transition/alias barrier and its preceding state. Preserve the first barrier
that introduces uncertainty with its preceding state and command-list order.
Later ambiguous transitions cannot overwrite that first cause. Reset starts
a new recording and clears its state entries. These are bounded scalar/API
records; diagnostic serialization never dereferences their saved pointers.

At marker consumption, copy the three input states and list order into its
existing retained slot. At the first relevant barrier batch, snapshot the three
states before and after applying that batch. Keep only the first accepted base
boundary, first accepted haze boundary and first rejected boundary. Each receipt
includes the whole already-bounded batch (at most 1,024 barriers), resource
identities, source frame/epoch, native queue size, list/Reset generation, marker
order and batch orders. No extra hooks, GPU commands or image captures are added.

The predicate still requires exactly two relevant whole-resource transitions:
color and motion, both RENDER_TARGET (4) -> PIXEL_SHADER_RESOURCE (128), no
flags. All three tracked input states must be known, issue-free and 128.
Diagnostics expose each component separately. The callback records rejection
before clearing the selection and latching fault 19. It does not attempt an SDK
evaluation on rejected inputs or retry the feature afterward.

The old TAA-dependent route evaluated at a native compute dispatch. It checked
color in NON_PIXEL_SHADER_RESOURCE and packed motion in the combined pixel/
non-pixel shader state within that consumer. Its success in the sauna does not
prove that the independent route's earlier barrier contract is valid there.

## Validation and next test

/W4 /WX build passed without compiler warnings. Thirteen targeted native checks,
66 AA Python checks and 16 startup checks passed. The startup suite requires its
fixture directory; an initial generic discovery invocation failed before running
tests, then the correct fixture invocation passed. Existing third-party CMake
deprecation notices do not represent new compiler warnings.

The new native fixture exercises the actual fault-19 observer callback with
owned WARP resource identities and a synthetic color-only batch. It verifies
marker/before/after states, exact rejection predicates, first state-conflict
preservation and snapshot immutability. No invalid GPU transition is submitted.
Python independently verifies its serialized decision. Existing queue, two-queue
GPU lifetime, early Reset and Reset/Signal race tests also pass. The SDK/rendering
pass is unchanged; no new SDK hardware run is claimed.

Session: `local/independent-aa-boundary-20260927/v0.8.3/`.
Use the same bare offline MO2 profile and **Monster Hunter: World - DLSS
1.2.110 test** entry. Keep native AA and DLSS Off, Windowed 2560x1440, DX12 On.
Load the save and remain outside the sauna until startup verification and
arming. After a baseline is captured, enter the sauna to reproduce the stop.
The diagnostic is continuous, with no fixed frame cutoff.

Use sealed arm.py/stop.py for the current process generation. If it faults,
save the durable closure and inspect the exact rejected batch. After normal
game exit use restore.py to restore previous.dll and remove the matching
sidecar. Freeze source/tests/tools through capture and closure. Archive with
`docs/research/independent-aa-boundary-result.md` and final current-state notes.

# Native request retirement after a publishing overlap — v1.19.1

The v1.19.0 live failure left frame 26473's request in slot 9 after a matching
native consumer passed through the publication guard. Frame 26505 then collided
with it. The same tracking logic existed in v1.18.4; its successful hour-long
test did not repair or reproduce this older fault.

## Correction

The publication guard still forwards native rendering without injecting DLAA.
For a single already-published TAA request, the hook now compares two bounded
packet reads against the complete stored packet and its recorded state, dispatch
dimensions and native TAA shape. It reserves that exact request with its unique
publication sequence. Its resource references remain in the occupied slot while
the original native consumer runs.

A POD token on the hook's stack identifies the reserved slot, sequence and packet.
On successful native return, the hook rechecks that identity and stable packet
contents, then clears that request and releases its tracking references. It does
not touch independent GPU recording pins or change command-list/fence retirement.
An explicit diagnostic stop still permits this in-flight bookkeeping to finish.

An exception, changed packet or stale token cannot erase another request. Failed
completion stops the feature (fault 10 for native exceptions, fault 32 for failed
return-identity validation). Nested consumers fail closed. Other consumers cannot
claim a request reserved by this native-forwarding callback, and private depth
capture excludes such requests. Reusing the same ring slot before the native
callback returns still raises the existing fault 6; the ring is not enlarged and
live records are not blindly overwritten.

Unmatched, ambiguous and invalid blocked callbacks continue native rendering
without inventing request consumption. In particular, a callback arriving before
its producer has published cannot be assigned to a future request from pointer
equality alone. This change fixes the observed already-published-request case;
the next live test must establish whether another overlap case occurs.

No native hook addresses, calling conventions or code gates changed. The relevant
consumer is the existing Win64 native function at RVA 0x259cc20; producer RVA
0x23c9980 and the existing callbacks retain their arguments and SEH forwarding.
Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The native call owns rendering; the diagnostic owns only its metadata and extra
COM references. The qualified DLAA GPU replacement itself is unchanged.

## Verification and scope

Release `/W4 /WX` builds pass. **34 focused native checks** pass, including 11 new
actual native-hook callback cases: the 32-frame wrap, a two-thread producer/
consumer overlap, premature reuse, stop, SEH exception, packet changes before/
after native execution, stale sequence, ambiguity, unmatched callbacks and
reentrancy. The success cases prove one native call, no injected GPU work, and
safe reuse after retirement. The earlier observation-only blocked fixture retains
its original failure behavior when invoked without the native-return tracking
scope; the new fixtures exercise that production scope through `native(1, ...)`.

Versioned `forwarded_requests` telemetry uses `validated-native-return-v1`, with
candidate/outcome partitions, pending and retired counts, and bounded first/last
retirement identities. The new reviewer validates these against blocked native
callbacks, checks monotonic counters, and requires no pending return at final
closure. Existing request, resolve, SDK, jitter and GPU-retention checks remain.

The diagnostic remains a qualified bypass with `exclusive_mode=false`. Native
forwarding during publication is still permitted at this stage; this fix is not
the final mode-wide AA-ownership implementation. CMFM/menu work remains deferred.
CPU fixture success does not establish live area/sauna stability. The next run
must verify both continued reconstruction and actual forwarded-request retirement.

Ignored build, test, source and deployment evidence:
`local/native-taa-bypass-20260927/v1.19.1/`. Its deployment script preserves the
baseline DLL and graphics file; the same temporary native TAA setting activates
the preparation stage. Use the existing [test procedure](../runtime/native-taa-bypass.md)
with this version's frozen controls and restore script.

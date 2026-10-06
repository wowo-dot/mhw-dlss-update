# Typed jitter property assignment

Updated 2026-09-23. [Run 37452](native-jitter-callback-result.md) shows TAA2
updates entering with jitter-enable already zero. This offline follow-up
identifies an indirect byte writer and a precise way to filter it to TAA2's
`mJitterEnable`. The subsequent [run 44144](jitter-property-writer-result.md)
observed two 1-to-0 writes returning through the mapped track-value branch.

All RVAs apply to host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decoded comparison is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Private disassembly and `verify_writer_map.py` are under
`local/jitter-property-writer-20260923/`. The script checks both identities,
eight bounded spans, 13 instructions, the property name, vtable slot and the
type-3 branch of the property copier. No live process was used for this map.

## Descriptor and setter

TAA2 registration `0x2391830` constructs a property descriptor naming
`mJitterEnable` at string RVA `0x3344478`, type/flags 3, owner equal to the
TAA2 object, storage equal to owner `+0x184`, and index zero. The descriptor
is allocated as a `0x58`-byte node. Relevant fields are:

| Descriptor offset | Meaning in this registered direct scalar property |
| --- | --- |
| `0x00` | Property name pointer |
| `0x10` | Type/flags, 3 for this boolean |
| `0x18` | Borrowed owning object |
| `0x20` | Direct storage pointer |
| `0x28` | Array length or method argument in other forms |
| `0x30` | Method pointer in indirect forms |
| `0x40` | Element index, zero here |

Generic setter `0x218F3F0` takes the descriptor in RCX and the byte value in
DL. Its exact reviewed extent is `0x6A` bytes, ending at `0x218F459`'s return.
Flags can choose method dispatch, an array bound check or a special property
form. For this registration's flags 3/index 0, the direct path writes DL to
`descriptor.storage + descriptor.index` at `0x218F456`. That is exactly
TAA2 `+0x184` when the registered descriptor is passed. The setter does not
need a literal displacement `0x184`, which limits direct-offset writer searches.

The scalar branch does not normalize DL; preserve its actual byte in evidence.
Some callers normalize their source before invoking it. Descriptor and owner
pointers are borrowed for one invocation; registration does not grant the
observer ownership or permission to retain and dereference them later.

## Static caller routes

| Route | Reviewed source of value | Setter call/return RVA |
| --- | --- | --- |
| Named assignment API `0x21711F0` | Builds the target object's property list, searches type 3/name, takes a byte from its supplied value pointer | `0x217124A` / `0x217124F` |
| Property copy `0x218A4C0`, direct boolean branch | Source descriptor storage plus source index; destination descriptor is preserved separately | `0x218A6A0` / `0x218A6A5` |
| Property copy, getter branches | Value returned by the source's accessor | `0x218A658`, `0x218A66A`, `0x218A689` |
| Track-style update branch | Byte from a referenced data array, masked with 1 | `0x221D5E8` / `0x221D5ED` |
| Stored-value assignment branch | Byte from a selected `0x40`-byte record | `0x221E0CC` / `0x221E0D1` |

These are alternative native routes into the same generic setter, not proof
that any one of them targeted jitter during a previous capture. The copier's
type switch is checked: type 3 selects the boolean branch at `0x218A634`.
There are also direct callers in other serialization/property systems and
tail-call routes. A setter return address can reflect a tail caller, so it
does not automatically identify the whole source chain.

The previously mapped scene stream reader `0x16E7990` sets its parameter
object's value at `+0x19`; metadata selection at `+0x18` is separate. Its link
to a live TAA2 descriptor is still unobserved. This pass does not substitute
presence for value or infer that native TAA is broken.

## Completed capture and follow-up

The [typed writer observer](../runtime/jitter-property-writer.md) records
only descriptors with the exact property name pointer and TAA2 owner type.
It requires flags 3, index 0 and storage equal to owner `+0x184`, then records
the incoming byte, before/after value, caller, thread and ordering. It forwards
the original setter without changing the requested value.

Run 44144 armed at the menu and continued through a user-reported save load
with DLSS Off/TAA. It captured two complete 1-to-0 assignments, both returning
to `0x221D5ED`, and stopped cleanly. Ten dropped admissions limit completeness.
The previous DLL is restored and the new output sidecar removed.

The [result and source follow-up](jitter-property-writer-result.md) map the
enclosing routine `0x221C9E0` through chained unwind entries. Its boolean kind
11 reads `values[selected_index] & 1` from a track record with the destination
descriptor embedded at `+0x10`. The exact source context was not captured;
these additional caller spans were checked offline only in that run. The
[v1.1.0 extension](../runtime/jitter-track-writer.md) completed that capture in
[run 42104](jitter-track-writer-result.md): both calls share one-key zero-value
source data, with live caller-code gates verified. Do not repeat an unchanged run.
No conclusion about raster jitter, motion vectors or GPU alignment follows
from this CPU property capture alone.

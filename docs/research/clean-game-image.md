# Clean game image (2026-10-11)

The decoded image this project checked code against until 2026-10-10 was
dumped from a running game with mods loaded. From 2026-10-11 on, gates, heads
and IDA work use a clean image. Source: the CMFM session's handoff
`D:\mh world modding\CMFM\docs\handoff-clean-image-20261011.md`.

## The two images

| | Old image | Clean image |
|---|---|---|
| Path | `D:\mh world modding\ai\glavenus\MonsterHunterWorld_decrypted.exe` | `local\clean-image-20261011\image.exe` (copy of `CMFM\local\runs\20261011-clean-dump\dump\image.exe`) |
| SHA-256 | `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c` | `a78ea69b0566414a42f16d0f8ff7d729e095041e2ae76a34cc02dbe0939f1d80` |
| Made from | a running game with mods loaded | a plain Steam start at the title screen, no mods, read-only memory copy (`PROCESS_QUERY_LIMITED_INFORMATION \| PROCESS_VM_READ`) |
| Exe | `c2ebbbd2…90ea` | `c2ebbbd2…90ea` (version 421810) |

The clean image's `.text` is identical to Fexty's 15.23 Ghidra dump and to the
older `D:\reverse engine jank\MonsterHunterWorld_dump.exe` (CMFM session).

## Confirmed here (2026-10-11)

- **The differences.** Our own byte comparison of the executable sections
  (`.text`, `.bind`) finds the same 29 places as the handoff's table. A tighter
  merge splits three of them in two, giving 32 ranges.
  - 22 of the places are function entries that jump out of the image.
  - The rest are NOP patches, flipped jumps and redirected calls.
  - Each range, with its old and clean bytes, is in
    `local\clean-image-20261011\diff-old-vs-clean.json`.
- **Code gates.** `sigscan.py gates` matches 182 of 182 on the clean image,
  the same as on the old one. None of the gate ranges in `src` overlaps a
  differing range, and none ends within 4 KiB of one.
- **Live-frame heads.** `sigscan.py audit` gives the same result on both
  images:
  - Main:Update (0x1af2b60) is unique.
  - Render:Submit (0x23a7f50) is unique.
  - Render:Publish (0x229a080) has 20+ matches; that is fine, because its
    address is pinned and gated.
- **Nothing refers to the 29 places.** No RVA or function address of the 29
  places appears in `src`, `docs`, `tools`, `tests`, or the IDA research
  outputs under `local\ida-native-dlss-20261008` and `local\ida_static`.
- **The clean IDA database** `local\ida-clean-20261011\mhw.i64` was checked
  through the IDA MCP:
  - Its input SHA-256 is the clean image's.
  - It has 291,903 functions and Fexty's names.
  - The three live-frame heads match.
  - Fexty's names for two of the hooks: Render:Publish is `sMhRender::move`
    and Render:Submit is `cD3DRender::endFrame`. Main:Update (0x1af2b60) has
    no name there.

## Consequences

- No gate, head, hook site or research result of this project depended on
  the old image's modified bytes.
- Dated research notes keep the old SHA-256 as their audit image.
- `local\ida-native-dlss-20261008\decrypted.i64` was built from the old
  image. It still shows the modified bytes at the 29 places, so new work uses
  the clean database.

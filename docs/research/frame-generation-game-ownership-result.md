# MHW presentation ownership map: accepted v0.6.0

Accepted 2026-10-03 on the existing DLSS Performance v1.25.44 / Reflex On
renderer. Main-menu activation, save loading and one area change completed
without a runtime fault. The user reported a clean scene through the area
change and confirmed normal exit after intentional stop. This establishes the
observed presentation owners; it does not establish frame generation in MHW.

Retail executable version 0.0.0.0, SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Probe DLL SHA-256:
`1d68f7c6cf92405a9b61bdbb5be1f43673bb395ddc0cae796b9a267fd9eca7ae`.
The hook ABI, qualification and bounded sampling contract are documented in
[frame-generation-game-ownership.md](frame-generation-game-ownership.md).
No new native hook offsets or full-code gates were introduced.

## Confirmed observations

The bounded map recorded 69,557 API observations in 22 buckets and 122 samples,
with no truncation. Every observed game queue/swap-chain creation, GetBuffer,
GetDesc, GetCurrentBackBufferIndex, ResizeBuffers and Present caller used thread
4648. This was also the complete Reflex frame owner. Calls made outside the
qualified game image, including diagnostic queue creation and worker GetDesc,
remain recorded but are excluded from this inference.

The native HWND swap chain was windowed, 2560x1440, BGRA8 UNORM (format 87),
three buffers, FLIP_DISCARD (4), on a DIRECT queue. Its flags were 2114 / 0x842:
ALLOW_MODE_SWITCH, FRAME_LATENCY_WAITABLE_OBJECT and ALLOW_TEARING. Initial
GetBuffer samples returned three distinct resource interfaces. Four successful
startup ResizeBuffers calls kept the dimensions/count/flags and changed buffer
identities. No chain recreation or resize was observed during the armed phase;
the area load retained the native chain. Qualified Present samples had one
matching Reflex ticket and one synchronous native presentation call.

Sampling preserves exact bucket counts, the first four observations and
power-of-two checkpoints. It does not prove that unsampled buffer identities
or lifetimes stayed unchanged. The observed common owner supports the owned
FG fixture's creation-thread routing contract for these calls, without proving
all future device/recreation paths or SDK worker ownership.

Final joined review:

- 3,676 successful DLSS evaluations; three skips, with no increase at the
  area-load checkpoint after save loading.
- 5,058 complete Reflex frames, one token/Sleep per frame, 30,348 ordered
  markers and 64 complete ordered driver reports.
- Zero runtime faults or request collisions.
- All 3,680 temporal GPU signals completed and recording references released.
- All 4,684 jitter changes and full render resolution restored.
- Driver Reflex Off confirmed, Streamline shutdown completed and pins released.

Independent bounded VM_READ checks before game exit verified all 37 native hook
heads and 46 full native functions restored: 778 reads, 75,163 bytes, zero
process-memory writes. Temporal COM binding-observer restoration was checked
by its telemetry; those observer heads are outside the independently compared
37-head list. Saved graphics settings were byte-for-byte unchanged. Native AA
and DLSS remained Off, and the window size stayed fixed. Performance and latency
improvement were not measured.

## FG runtime scope and checker correction

The Streamline FG plugin `sl.dlss_g.dll` was not loaded. The probe made no FG
initialization/tagging calls and produced no generated game frames. The existing
NGX runtime did have `nvngx_dlssg.dll` mapped. Its SHA-256 was
`135eaf0733c1e37381a8c28abcf7a862404a54132b81787c04e35d09efc5e36f`,
matching the separately pinned owned FG foundation runtime exactly.

The first independent checker incorrectly rejected any mapped NGX FG DLL
before checking code restoration. Both that failure and the corrected checker
are preserved. The corrected checker requires the Streamline FG plugin absent
and records the existing NGX FG module/digest; the native-code comparisons are
unchanged. A mapped NGX runtime alone is not evidence of active frame generation.

## Evidence and continuation

Private evidence: `local/frame-generation-game-20261002/game-01-archive`,
38 hash-verified files, manifest SHA-256
`f37e5d87a0a7e1c0fe915b9dc78708b0eac8b536422c1de96cf320ca9fec2c85`.
The 679-file v0.6.0 source freeze has manifest SHA-256
`7a7237c33e9e7e917561d69d117f8bdab94bddfebde69a3120c38caa906584e4`.
The live run folder retains deployment/removal instructions and exact backups.
The current diagnostic DLL remains installed as requested. Original game files
and assets were untouched; graphics/runtime/mode configuration stayed unchanged.

Next, qualify the final post-processed HUDless scene image and UI composition
boundary, reusing existing temporal resource observations. Match camera,
depth/motion and the shared Reflex token, and establish producer/consumption
fences and input lifetimes before FG tagging. The early DLSS resolve output is
not automatically the final HUDless image.

The FG SDK also changes swap-chain pacing/lifetime. The accepted Reflex-only
stop/shutdown sequence cannot be reused for an FG-owned chain: owned controls
required chain retirement and a fresh native chain before native rendering
continued. The game's corresponding lifecycle remains unresolved. Ordinary FG
in MHW precedes MFG; the separately accepted owned 2x/3x/4x/6x foundation,
performance work, presets and CMFM keep their existing scope.

# Native half-scale control: verified first run

2026-09-27, probe v1.22.1, PID 43572, process creation 134350067555165293.
This is the first successful native active-scale test, independent of DLAA/NGX.
The executable identity, six native code gates, frozen build and configuration
are recorded in the preparation/deployment records.

## Observations

- Native display dimensions stayed 2560x1440; applied renderer/view active
  scale became 0.5, giving a planned 1280x720 active rectangle.
- There were 19,692 overrides and 19,692 observations of the applied half
  scale, with zero faults, unqualified visits or foreign requests. Each previous
  pending override was restored before the native scale policy ran again.
- The read-only D3D12 viewport hook recorded 230,883 half-size viewport calls,
  77,766 full-size calls and 725,867 other calls. These are API-call counts,
  not frame counts or a complete map of scene passes.
- All content/allocation scales stayed 1. Main-view depth CPU metadata stayed
  2560x1440. Native width override was enabled with an exact 2560x1440 basis.
- User reported that the scene filled the window and HUD/text looked normal
  while slowly panning. No cropping/flickering problem was reported.
- The agent requested stop after 95 seconds. All nine reported scale values
  returned to 1, with 12 subsequent full-size restoration observations. Both
  hooks were disabled, no override remained owned, and pending callbacks were
  zero. User confirmed the scene looked normal again and exited normally.
- Process exit and unchanged graphics/profile/mode/sidecar identities were
  verified. The v1.22.1 DLL remains installed and unarmed for the next process.

The capture and frozen reviewer establish native scale control and restoration.
Actual GPU texture descriptions, exact viewport-to-scene attribution, and
reduced color/depth/motion alignment remain unverified by this probe. There
were no DLSS evaluations, injected GPU commands or saved settings writes.
The last periodic `.status.json` predates stop; the final JSONL record and review
are the authoritative final state.

## Evidence

Root: `local/render-resolution-20260927/v1.22.1/`.
DLL SHA-256: `1c74e921d9cacd443cf205bb5fae61c3c242bfad325ff71870bc91404f2e71db`.
Final log: `final-43572-134350067555165293.jsonl`, SHA-256
`d6c7ee54f4a3a21927055ae5f6a460589f5cd5b78e8b832cc51b3e5a8541dc81`.
140-file final archive: `archive-final.json`, SHA-256
`b1fb2f32a8c6f1f0db0ac94e6bb049d127a33f55d3ea176a3e1dff0fc3e3cd92`.
99 frozen sources/dependencies and all prelaunch files reverified. Source,
native fixture, reviewer and process-generation tests passed before deployment.

## Next integration step

Carry the proven engine scale control into the modern temporal path while
separating display size, allocation extent, active input rectangle and resolve
output. Confirm the reduced color/depth/motion region at the native resolve
boundary before constructing unequal NGX input/output dimensions. Jitter and
motion conversion must use that active-input pixel convention; the reconstruction
destination stays full size. The existing equal-size DLAA runtime is unchanged.
Recovered depth-copy gaps remain parked for integrated beta unless they block
this work. Reflex and FG/MFG follow the resolution-changing DLSS integration.

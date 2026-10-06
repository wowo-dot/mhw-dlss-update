# Modern SDK initialization before native NGX: accepted

Run 26296, creation `134348341271819904`, 2026-09-25, v1.12.1.
Host 0.0.0.0 SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Diagnostic SHA-256
`1155702fd252e1a5dbd702dedb9f65b116b64974316ca8f8e1cf12420a572943`.

The four unchanged code gates initially failed (mask 0), then all matched
(mask 15) after 547 ms and 37 polls, while all seven native dispatch slots
were still zero. The sole startup hook installed before the native Init call.
This directly demonstrates delayed readiness in this run; it does not identify
which gate or underlying mechanism caused the earlier v1.12.0 rejection.

Public project-ID initialization succeeded with API 21. Availability,
driver-update requirement and feature-init result were `(1,0,1)` both before
and after the original native API-18 Init, which returned success unchanged.
The signed modern provider's full path and pinned SHA-256 were verified.
Later module enumeration independently matched the installed diagnostic and
modern provider. NGX also loaded its DLSSG/DLSSD providers during discovery;
their presence is not evidence of frame generation or ray reconstruction use.

The game reached a responsive main menu. The later graphics snapshot matched
the startup device, adapter LUID `(0,97816)`, 2560x1440, swap-chain format 87.
One callback completed, the hook disabled, and no callback remained active.
No feature was created, no image was evaluated, and no project GPU commands
were submitted. SDK/device ownership was retained until process exit.

The frozen full reviewer accepted the run. The user exited normally; the
exact previous DLL was restored at 21:27:07 +03, with all four configuration
hashes unchanged. Evidence: `local/ngx-startup-20260925/v1.12.1/`, 90 files.
Archive SHA-256:
`ec9e90a5e3491933a1e14b75b45c835378e01dd40d29f380f15b123382842ff4`.
Log SHA-256:
`440f2156de89f15d62f130cffb5b7e81ab14bd18d58f017683798ed5d32d5872`.

This establishes the capability initialization route on the live game device.
Next is [bounded live evaluation using that retained ownership](../runtime/startup-live-dlaa.md).
It does not establish live DLAA image evaluation, continuous history, visual
quality or replacement of the game's presented TAA image.

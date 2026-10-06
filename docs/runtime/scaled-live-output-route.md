# v1.24.5 game test

Use the existing barebonesdlss MO2 profile and the DLSS test launch entry.
Keep native NVIDIA DLSS, Anti-Aliasing and MotionBlur Off, with DX12 enabled and
the current 2560x1440 window unchanged. Test outside public multiplayer.

The diagnostic DLL occupies the existing dlssTEST slot. Its output-dir sidecar
points to the versioned repository evidence directory, which also contains
sdk-runtime-dir.txt for the already installed modern SDK runtime. Startup
initialization runs before the game finishes loading; gameplay hooks wait for
the explicit process-generation arm event. Launch, load a save and report that
the scene is ready before arming. There is no reason to toggle the native DLSS
setting. A short pause or momentary scale change is possible during capture.

This captures three input frames and attempts a full-output DLSS replacement
on the third. It restores full render scale and removes the runtime hooks at
the end. It will not leave continuous DLSS running afterward. Confirm that the
scene is normal and exit normally after evidence has been saved. Keep the DLL
installed as requested; the next launch again waits for a fresh arm event.

Arm with tools/arm_scaled_temporal.py using the exact input log, DLL SHA-256 and
a new receipt path. Review with tools/review_scaled_temporal.py after shutdown;
it also verifies the matching startup log. Preserve partial/failed logs.

The versioned deployment directory contains previous-slot.dll.disabled and
output-dir.before.bin. With the game closed, its deploy.ps1 -Action Restore
restores those exact previous slot contents with identity checks. Installation
and removal never change the game EXE or original assets. Do not restore
automatically after this run; the user's preference is to retain the test DLL.

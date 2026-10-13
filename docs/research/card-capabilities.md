# What the graphics card offers (v0.27.5, 2026-10-14)

Groundwork for the menu rework: the menu offers only what this card, driver
and Windows support, and says why the rest is missing. The answers come from
the SDKs, never from card names.

## The model

`src/alpha/capabilities.hpp` (pure, tested in `tests/alpha_menu_tests.cpp`):

- the card: DXGI's adapter description (name, vendor, device, dedicated video
  memory) and the driver version Streamline detects;
- DLSS: available once NGX's startup and the DLSS install succeed at arming
  (`ngx_startup` refuses without `SuperSampling.Available`);
- Reflex: `slIsFeatureSupported(Reflex)` and `ReflexState::lowLatencyAvailable`;
- frame generation: `slIsFeatureSupported(DLSS_G)` and
  `slGetFeatureRequirements(DLSS_G)`; a failure maps to the reason the player
  reads: card (adapter or feature not supported), driver (with the required
  and installed versions), Windows, Hardware-accelerated GPU scheduling,
  `frame_generation_plugin = off`, or other (the log keeps the SDK's result);
- the multiplier limit and Dynamic: `DLSSGState::numFramesToGenerateMax` and
  `bIsDynamicMFGSupported` (DLSS-G guide 14.1, 14.2), read right after attach
  while DLSS-G is loaded (also on a start with frame generation off, which
  unloads it afterwards), or from the session's first state read.

`available(caps, option, value, why)` decides each card-dependent choice (a
DLSS mode, frame generation, a multiplier, Dynamic, Reflex); unknown support
allows it. `card_line` and `features_line` are the Status page's new rows
("Graphics card", "Supports").

## Where it is filled and used

- `presentation_probe.cpp`: the card at the device's attach (before any check
  that can fail), Streamline's answers and DLSS-G's limits after it, logged
  as `card_capabilities` and `fg_limits`; DLSS at arming; the limits again
  from the session (`fg_limits`, source `session`).
- `status.cpp` keeps it with the Status view; the menu's validate and lock
  callbacks ask `available()` (the old `multiplier_allowed` and
  `dynamic_allowed` are gone).

## Next

The menu rework (designed with the user): switches and choices built from
these answers, the outdated texts from game-129 rewritten.

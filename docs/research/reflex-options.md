# Reflex options (v0.20.0)

Roadmap step 2, "DLSS wholesale". Reflex was always Low Latency. Players can
now choose it.

## The modes

| ini `reflex` | Menu | `sl::ReflexMode` | Meaning |
|---|---|---|---|
| `on` (default) | On | `eLowLatency` | Reflex Low Latency: a short frame queue |
| `boost` | Boost | `eLowLatencyWithBoost` | The same, plus the driver keeps GPU clocks up when the CPU is the limit |
| `off` | Off | `eOff` | No Reflex. Only while frame generation is off |

DLSS Frame Generation needs Reflex. Three places enforce that:
- **The ini:** `reflex = off` with frame generation on is read as `on`, with a
  warning. The rule is applied after every key is read, so key order does
  not matter.
- **The menu:** its validate step refuses Off while the frame generation
  setting is on. Turning frame generation on lifts a saved Off to On, live.
  When DLSS-G was loaded at start, an Off chosen after frame generation was
  turned off for the next start is saved but not applied: Reflex stays on
  until the game closes.
- **The worker:** `apply_reflex_mode` refuses Off while DLSS-G is loaded and
  logs `reflex_mode_refused`.

## How it applies

- **At start:** the ini mode is the requested mode (`reflex_mode.hpp`, an
  atomic). Arming calls `slReflexSetOptions` with it instead of the fixed
  `eLowLatency`. If frame generation is loaded, Off falls back to On.
- **The logs:**
  - `mode_options` records the mode and `boost_requested`;
  - `frames_armed` and `frame_hooks_installed` record the armed mode;
  - `alpha_config` writes `reflex` as its ini value.
- **Live:** the menu's choice becomes the requested mode. The worker loop
  (100 ms) compares the requested and applied atomics, and on a change takes
  `sdk_mutex` and calls `sdk.options`. It logs `mode_options` with phase
  `live` and updates the status page. A refused or failed change goes back to
  the applied mode.
- **The driver check:** the first heartbeat requires the driver's low-latency
  flag to match the applied mode (on for On and Boost, off for Off), instead
  of always on.
- **Off keeps the frame hooks:** with Reflex Off they stay installed. Their
  markers and sleep calls are harmless with the mode off, and the latency
  report keeps working.

## Menu and status

- **Settings:** a new section, "Latency", with "Reflex" (On, Boost, Off),
  applied at once and saved to `mhw_dlss.ini` when the page closes, like the
  other keys.
- **Status:** "On (low latency)", "Boost (low latency, GPU clocks kept up)"
  or "Off". The player's Off is not counted as degraded; a Reflex failure is
  still "Off: why", and "Degraded".

## Tests

- **`alpha_config_tests`:**
  - each mode, in any case;
  - Off with and without frame generation, in either key order;
  - an unknown value;
  - the ini values round-trip;
  - `with_reflex` edits in place, and an appended key reads back.
- **`alpha_menu_tests`:** the choice order and indices, the Off rule and its
  reason, the `sl::ReflexMode` values, the three status lines, and that a
  failure still degrades.

## In game (not yet seen)

Boost and Off have not run in game yet. To confirm:
- a live On → Boost → On switch: `mode_options` with phase `live`, then
  `driver_sleep_status`;
- a start with frame generation off and Reflex Off: `frames_armed` mode 0,
  and the driver reporting low latency off.

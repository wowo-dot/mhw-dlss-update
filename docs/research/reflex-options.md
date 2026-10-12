# Reflex options (v0.20.0 to v0.20.2)

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

## In game: game-79 (v0.20.1, 2026-10-13)

The Settings page shows again (CMFM: "Settings" added, 7 settings). This run
used DLAA at 2560x1440, with three launches. Every launch ended with
`game_exit`, with no Reflex fault and empty crash logs.

| Launch | Start | Live changes | Reflex frames | DLAA frames |
|---|---|---|---|---|
| 1 | FG 2x, Reflex On | On → Boost → On | 15,213 | 8,314 of 8,315 |
| 2 | FG 2x, Reflex On | menu: FG Off, Reflex Off (next start) | 5,616 | n/a |
| 3 | FG off, Reflex Off | Off → On → Boost | 20,900 | 15,991 of 15,992 |

- **Live changes:** each was one `mode_options` with phase `live` and result
  0, with `boost_requested` set only for Boost. CMFM logged the same choices.
  FG kept generating through On → Boost → On.
- **Launch 2:**
  - the menu took Off only for the next start;
  - it logged "frame generation runs until then";
  - the worker made no live call;
  - the saved ini read back as `frame_generation = off`, `reflex = off`.
- **The Off start (launch 3):**
  - `mode_options` activate mode 0 and `frames_armed` mode 0;
  - the driver reported `low_latency` false at `running_on`;
  - DLSS-G was not loaded (`fg_unsupported` `plugin_off`);
  - the frame hooks kept running in Off (sleeps = frames), as designed.
- **Latency, launch 3 (PCL ranges in µs, one 20 s window each):** these
  are not a benchmark, since the scene changed between windows.

  | Mode | queue | driver |
  |---|---|---|
  | Off | 3,802–5,218 | 421–1,377 |
  | On | 3,190–4,080 | 271–893 |
  | Boost | 3,460–4,518 | 283–932 |

  The scene was GPU-bound at about 240 fps, so Boost has little to do there.

The phase names `activate_on`, `requested_on` and `running_on` predate the
modes and are kept for log compatibility. With Off they carry mode 0 and
`low_latency` false.

### The driver check after a live change (v0.20.2)

game-79 showed that only the first heartbeat asked the driver for its mode,
so a live change was never confirmed. Now, after a change applies, the worker
takes the sleep count. Two sleeps later it logs `driver_sleep_status` with
phase `live`, under the first heartbeat's rule (low latency on for On and
Boost, off for Off). A mismatch fails closed with `driver-live-mode`, the same
as a startup mismatch. The driver's sleep-status structure (version 1) has no
Boost flag, so Boost is confirmed only as low latency on.

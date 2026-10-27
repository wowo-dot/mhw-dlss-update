# NVIDIA's PC Latency ping (0.32.0, 2026-10-27)

The handoff of 2026-10-27 listed a missing "optional `eInputSample` marker".
That marker no longer exists.

## What Streamline 2.12 says (confirmed, its headers, guide and MIT source)

- `sl_pcl.h`: `//eInputSample = 6, // Deprecated`. The PCL guide (4.0,
  migrating from SL Reflex): "`eInputSample` marker was removed, this was
  already deprecated and removed in the native Reflex SDK".
- PC Latency = input sampling latency + simulation start to present +
  present to displayed. The input sampling part comes from a **ping**:
  Windows messages with id `PCLState::statsWindowMessage` are sent to the
  application; on one, the application sends `ePCLatencyPing` with the frame
  index of the frame that picks up the input, or sets a flag for the next
  simulation to send it (PCL guide, introduction and 2.0).
- `sl.pcl/pclstats.h` (`PCLStatsPingThreadProc`): while an ETW consumer has
  PC Latency stats enabled, a thread wakes every 100 to 300 ms; with
  `PCLOptions::idThread` 0 (the default; the mod sets no PCL options) it
  posts `statsWindowMessage` to the foreground window if that window belongs
  to the process.
- `eControllerInputSample` (13) exists in the enum, with no description in
  the 2.12 guide; the mod does not send it.

So without the ping, the NVIDIA overlay's and PresentMon's PC Latency lack
their input-sampling part. The ping changes measurement, not latency.

## In the mod

- The mod's markers (`reflex/frame_session.cpp`): sleep and simulation start
  at the game's frame update (Main:Update, where it also reads its
  DirectInput and XInput state), simulation end and render submit start at
  Render:Publish, render submit end at Render:Submit, present start and end.
- The ping message reaches the game window, which Astera Loader subclasses;
  `al.input` (API 1.6) gives plugins its messages before the game. The mod
  subscribes at its init (`plugin/plugin.cpp`), never consumes the message,
  and on it calls `reflex::request_latency_ping()`; the next simulation start
  sends `ePCLatencyPing` with its own frame token. A refused ping is counted
  and does not stop the frame's markers.
- The vendored Astera Loader header moves from API 1.4 to 1.8
  (`third_party/al/README.md`); every Astera Loader since v0.2.0-dev.1
  provides it, and the mod requires 0.2.2. Requesting `al.input` makes Astera
  Loader install its input layer, which CMFM already does.
- The frame heartbeat logs `latency_pings`: subscribed, the message id,
  pings seen and sent, and refusals.

## Not checked yet

- That pings arrive in game while a PC Latency consumer runs (the NVIDIA
  overlay's PC Latency, or PresentMon 2.x), and that the readout then
  includes the input part.

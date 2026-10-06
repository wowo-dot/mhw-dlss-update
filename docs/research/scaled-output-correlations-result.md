# v1.25.7: panning resets, sauna haze, and early CPU recordings

Local test date: 2026-09-30 (Europe/Istanbul). This diagnostic run separated
three conditions that had previously been grouped under visible DLSS skips.
No rendering correction was installed during the capture.

Evidence: `local/continuous-dlss-20260929/v1.25.7`, game PID 23652, creation
FILETIME 134351908442056289. Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded comparison SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
DLL SHA-256:
`dd9566f6887a9ebc9da94fb18c4a0be1b73f0f8ec6ba6fe6eda97e1edeb04ff8`.
Native AA, DLSS and MotionBlur Off; DX12; 2560x1440 window; Performance mode
1280x720 reconstruction input. Latest DLL remains installed by user preference.

## Isolated panning: history resets without skipped evaluations

Baseline: 1,043 successful evaluations, one startup skip, one initial reset.
The next checkpoint followed the requested camera pan outside the sauna with
menus closed. The user reported brief visible skips. It contained 2,398
successful evaluations and still only the startup skip. No output-order or
consumer-depth rejection had occurred in this checkpoint interval.

There were 241 additional camera-displacement history resets. All other reset
reason counters stayed zero apart from the initial reset. Retained examples
moved the solved world-space camera center 22.4866 units over 34.634 ms and
22.5697 units over 36.166 ms. `continuous_dlaa/history.hpp` currently resets
above a fixed 20-unit displacement per frame. Normal third-person camera
orbit can move that center; the test's lower frame rate also affects the
distance between samples. Neither observation establishes an appropriate
replacement threshold.

Repeated resets are a candidate cause of the visible panning instability.
A controlled comparison is still needed. This checkpoint does **not** support
calling the symptom a skipped SDK evaluation or an output-order rejection.
The complete run ended with 883 resets, including 860 camera-displacement
reasons; reasons can overlap, so their counts must not be summed as reset totals.

## Sauna: an identified extra pass changes the connection

The user reported DLSS stopping/changing inside the sauna. Successful
evaluations stopped advancing at 3,381 while unbuilt-plan rejections grew.
Leaving the sauna restored DLSS both visually and in telemetry.

The five retained rejected graph receipts all have reason 6/detail 4 and
`link_reads=7`. Motion input/output and grade input were read successfully;
grade input differed from motion output. Grade output was not read after this
comparison failed, preserving the existing short-circuit behavior.

An extra stage-15/order-20000 descriptor has type RVA `0x35372b0`, the previously
identified `uHazeFilter`. Its input equals motion output and its output equals
the different grade input. In symbolic owner names, the observed connection is:

| Path | Motion output | Haze | Grade input |
| --- | --- | --- | --- |
| Successful baseline | S0 | absent | S0 |
| Retained sauna rejections | S0 | S0 to S1 | S1 |

This confirms an intervening native pass rather than a failed pointer read.
The prior callback/type mapping is documented in `independent-aa-haze-queue.md`:
callback RVA `0x24c7760`, 2,256 bytes, SHA-256
`d1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701`.
The current scaled route only builds the direct motion-to-grade chain.
This run did not capture all haze shader bindings or prove its expanded-output
resource states; accepting the changed link alone would be insufficient.

## Recovery: CPU callback order differs from submission order

Seven output-readiness decisions rejected already completed downstream CPU
draw callbacks after the sauna period. The bounded recorder retained cases
1, 2, 3, 4 and 7, with executed masks 4, 4, 7, 4 and 6 respectively.

In all five cases, the temporal recording was list index 1 of a three-list
batch. Every stage rejected as already recorded belonged to list index 2 of
that **same queue and submission batch**, with an independently matched list
generation. Other stages either shared the temporal recording or were also
on list index 2. Submission timestamps followed the readiness decisions.
The callback-time native frame/epoch was one ahead of the plan's frame/epoch
in these receipts; that global counter alone must not replace plan identity.

This establishes early CPU recording, not early GPU consumption. The current
replacement logic depends on reaching the temporal callback before those
native draw callbacks. Supporting parallel recording requires choosing and
preparing the replacement before affected draws are recorded, while preserving
GPU dependencies, resource state validation and retirement. Simply removing
the executed-mask guard cannot retroactively replace native draws already
recorded into another list. These receipts do not alone prove a new route safe.

## Accounting and cleanup

The stopped log and individual channels pass the versioned reviewers:

- 4,164 successful DLSS frames; zero runtime/SDK faults.
- 16,468 unfiltered frames: 16,438 unbuilt output plans, seven output-order
  rejections, 22 consumer-depth interval rejections, and one startup warmup.
- Six suspension periods, all resumed before intentional stop.
- All 20,610 reconstruction/depth GPU recordings completed and released;
  all 16,468 unfiltered GPU recordings completed and released separately.
- All 20,636 jitter changes restored; full render scale returned.
- Runtime reports all hooks disabled; read-only live verification matches
  all 36 full native code gates and 17 native hook heads. COM hooks are covered
  by runtime cleanup, not by that native-head comparison.

The depth reviewer identifies 22 interval rejections. None remained in the
bounded failed-sample slot when the saved observations were taken, so this run
does not supply their exact failed command indices. Earlier v1.25.6 evidence
remains relevant but must not be relabeled as a v1.25.7 sample.

The user confirmed the scene was normal after intentional stop and exited
normally. Closure/archive checks preserve settings, installed DLL and sidecar.
Successful capture verification is not a visual stability pass: the reported
issues remain to be corrected. Performance cleanup stays deferred.

## Next implementation work

Use these receipts as regression fixtures for optional haze routing and for
replacement planning before parallel CPU draw recording. Map the additional
pass's bindings and depth use before expanding admission. Independently
revisit the fixed camera-displacement reset rule using ordinary orbit versus
real discontinuities, retaining frame/resource/projection checks. Validate
each behavior with a focused comparison rather than attributing every visible
skip to one cause.

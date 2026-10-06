# v1.25.42 combat validation — 2026-10-02

The targeted Safi nova and Alatreon frost/nova checks passed visually. The user
reported DLSS remaining active throughout each. The recorder observed no output
graph rejection, overlay rejection, radial binding failure or runtime fault in
this run. This validates the tested paths, not all post-processing effects or
uninterrupted reconstruction across every transition.

Evidence: `local/radial-native-contracts-20261002`, PID16396, creation
134353690532440251. See [implementation](radial-native-contracts.md) and the
[preceding failure run](postprocess-family-validation-result.md).

## Results and limits

- 23,340 DLSS evaluations; 2,972 copy/radial draw pairs and 5,972 completed
  overlay draws. No graph/overlay rejection and no runtime fault.
- Safi's before/after nova checkpoints added 739 radial draws and 1,724 DLSS
  evaluations without a new skip or suspension. Two accepted paired CPU
  snapshots (native frames 26201 and 26203) captured the format-7, 1x1 default
  radial mask: t1/t2 share the exact owner, view and descriptor. The known
  scene-copy-radial resource chain and shader hashes match.
- Alatreon's frost checkpoints added 499 radial draws and 3,018 DLSS
  evaluations with no new skips/rejections. A bounded reader obtained one
  accepted paired snapshot at frame 38262, again showing the same default mask.
  It did not obtain the requested second sample within its 120-second bound.
- The later nova checkpoint added 422 radial draws and 885 haze draws. One
  pipeline warmup rejection (stage 3, missing pipeline bit 8) appeared in that
  wider interval and recovered. The user still reported a clean, active image.
  Message times are not exact video-to-native-frame alignment.
- 71 skip attempts occurred over the whole run, with ten suspension periods
  and ten resumptions. A 62-attempt burst was recorded before the frost report;
  no output graph or overlay rejection accompanied it. Its exact gameplay cause
  was not established. Do not describe the entire run as zero-skip.
- Two same-owner auxiliary-storage refreshes completed; no refresh or API
  binding rejection. The two entry comparisons describing changed storage were
  handled by the existing refresh path.

Current stable CPU captures each contain one descriptor and two draws. They
directly exercise the default-mask correction. The equivalent-output,
multiple-descriptor rule passed offline graph tests, and its former ambiguous-key
failure did not recur, but this run has no separately accepted live snapshot
proving two descriptors were present. Shared-key counters alone do not prove
that distinction.

CPU readers did not suspend the game, write process memory or decode GPU
descriptor contents. Frame-local GPU contracts remain validated by the in-process
code. No complete combat GPU-frame replay or objective temporal-quality metric
was produced. The hunter silhouette and camp-entry reports remain separate work.

## Stop and preservation

The assistant announced an intentional stop. The terminal receipt reports a
healthy stop. All 23,411 main GPU signals completed; all 23,410 main, six
unfiltered and 27,621 output recordings released their references. All 24,119
jitter changes were restored, full render scale returned, and native code was
read back while the process remained alive: 40 complete spans and 18 hook heads
matched. Frozen full review, fourteen component reviews and all 144 timed
snapshots passed their checks. The collector ended on the native terminal event.

The final `closed-result-16396.json` and `archive-final.json`, when present,
record normal-exit confirmation, process absence, unchanged configuration/DLL
identities and archive integrity. Keep v1.25.42 installed as requested; a later
launch needs a fresh output folder. No original executable or asset was changed.

# Recording state and in-place overlay extent, v1.25.21

This candidate addresses two separate observations in the v1.25.20 broad run:
quest-abandon animation/menu transition lost a known output state, and cutscene
overlays covered a top-left 1280 x 720 rectangle on a 2560 x 1440 output.
See `copy-branch-output-broad-run-result.md` for the failed run and cleanup.
The candidate has not yet been verified in the game.

## Evidence and limits

The failing draw retained the expected resource, RTV, pipeline and root
signature. Its local recording had no known output state (receipt mask 2).
The old barrier listener ignored native transitions outside selected callbacks.
That is a confirmed observation gap; it does not prove which missing transition
caused the recorded failure. No inference from an RTV binding replaces a state
transition, and state is not inherited across independent command lists.

The captured vignette and light-shaft composite used a full-size color target
with a half-size viewport. Their captured pixel shaders use UVs; they do not
consume screen position. Both use the same captured passthrough vertex shader.
The vignette's shape uses UV and aspect ratio. Expanding its raster extent while
keeping its existing constants and UVs is therefore the candidate correction.
The screenshots support this explanation but are not exact-frame matched to
the later shader capture. Raw native vertices were not captured in that ad hoc
inspection; the candidate qualifies them during preparation.

## Recording observations

The existing D3D12 ResourceBarrier listener now observes native transitions for
resources already pinned by prepared plans, including outside selected native
callbacks. A bounded journal keys each observation by command-list identity
and successful Reset generation. New plan recordings import only that exact
generation's observations. Aliasing, split and partial transitions invalidate
knowledge; contradictory transitions reject the route. Reset releases the
journal's observation references. Existing injected GPU uses remain retained
until both Reset and fence completion. Hook shutdown clears only journal refs.

No new hook or executable offset is introduced. The tested host executable is
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Existing native wrapper calling conventions and ownership checks are unchanged.

## Overlay admission and draw

Up to eight additional draws may join a prepared frame. Discovery uses the
completed native queue, descriptor flags, sort order and exact output ownership;
it does not use scene names or vignette/light-shaft type identifiers. An overlay
must write in place to the immediately preceding color output. It must use a
four-vertex full-clip triangle strip, the captured passthrough VS, and a pixel
shader whose used inputs are TEXCOORD0.xy with one SV_Target0 output. Native
auxiliary textures are permitted; sampling a reconstructed color resource is
rejected because it needs a different UV-domain contract. Partial geometry,
screen-position shaders and unsupported packets reject admission.

The overlay keeps its native shader, PSO, blend, input buffer, UVs, constants,
descriptors and render target. Under the same immutable frame predicate as the
existing chain, the native branch draws with the original extent; the full
branch expands viewport/scissor to the display extent. Both raster settings are
restored afterward. All overlay packets participate in the frame submission
order and retained recording lifetime. Reference capacity is 96 per recording,
bounded and checked atomically, to include additional pipelines/buffers/heaps.

Plan reset constructs the retired slot in place, avoiding a large temporary on
the game worker's stack. No game settings or native AA selection are changed.

## Verification and next runtime check

Versioned `recording_states` and `overlays` channels distinguish observed
transitions, imports, prepared overlays, recorded draws and completed frames.
The reviewer includes overlays in completion accounting and rejects missing
channels for v1.25.21. Overlay executed bits extend to 12 slots; the four private
effect pipeline identities retain their existing meaning.

Offline checks exercise generation isolation, contradictions, invalidations,
bounded capacity and reference release; real GPU tests exercise unscoped
barriers, CPU/GPU ordering, native/full predicates, restoration and retirement.
A captured-shader pixel comparison checks the expanded vignette against an
independent native full-size draw with blending. Its half-size control reproduces
the sizing error. Native-structure fixtures reject unsupported overlay inputs.
Private shaders and all generated evidence remain under ignored `local/`.

Runtime acceptance still requires the reported cutscene/nova rectangle and
quest-abandon transition, followed by the agreed broader effects test. A passing
offline comparison is not a claim that every native overlay is qualified.
The DLL stays installed after testing unless the user requests restoration.

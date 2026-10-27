#pragma once
#include <d3d12.h>
#include <iosfwd>
#include <string>

namespace mhw::temporal_present {
struct Bindings;
}

// The player's Sharpness (0.32.0): NVIDIA's NVSharpen on the frame's
// HUD-free final scene, while this mod's DLSS draws (docs/research/nvsharpen.md).
// The runtime prepares it at install, records it in the final-scene observer
// before frame generation's HUD-less copy, and resets it at its drained
// rebuild. Without it (or at 0 %) the frame is as before.
namespace mhw::sharpen {

// Any thread: the Sharpness, 0 (off) to 100 %, live.
void percent(unsigned) noexcept;
unsigned percent() noexcept;
// Worker, at the runtime's install: compiles and creates the pass, once per
// process. False with the reason: no sharpening this session.
bool prepare(ID3D12Device *, std::string &why) noexcept;
// No recorded work can still run (the runtime's drained rebuild).
void reset() noexcept;
// The frame's HDR10 output as the game's CBRenderFrame takes it (renderer
// +0x21A, +0x21C). Not known: not sharpened (an SDR encode would clip HDR's
// highlights).
struct Hdr {
    bool known{}, on{};
    float white{};
};
// Final-scene observer (stream_route::FinalSceneObserver contract): sharpens
// `scene` and leaves it, and the list's bindings, as given.
void record(ID3D12GraphicsCommandList *, const temporal_present::Bindings &, ID3D12Resource *scene,
            D3D12_RESOURCE_STATES scene_state, Hdr) noexcept;
// The log's "sharpen" object.
void write(std::ostream &);

} // namespace mhw::sharpen

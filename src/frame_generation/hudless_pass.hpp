#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

// HUD-less colour for DLSS-G: the HUD-free final scene target T (linear
// R11G11B10_FLOAT, before any UI draw) through MHW's screen transform
// (screen_transform.hpp), written to an FG-owned R8G8B8A8_UNORM slot texture.
// The brightness parameter is read on the GPU from the game's own CBSystem
// constant buffer bound to the replaced final-screen draw. A per-slot probe
// records the CBSystem values the pass actually used.
namespace mhw::frame_generation {

class HudlessPass {
public:
    static constexpr unsigned slots = 3;
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    struct Probe {
        float gamma_ex{}, gamma{};
        bool parameters_ok{}, valid{};
    };
    // Throws on any creation failure; the caller then runs without HUD-less.
    HudlessPass(ID3D12Device *device, unsigned width, unsigned height);
    // Records on `list` with the caller's bindings already saved. Transitions
    // `scene` RENDER_TARGET -> NON_PIXEL_SHADER_RESOURCE -> RENDER_TARGET and
    // `target` from/to `target_state`. Changes compute root, PSO and heaps.
    void record(ID3D12GraphicsCommandList *list, unsigned slot, ID3D12Resource *scene, ID3D12Resource *target,
                D3D12_RESOURCE_STATES target_state, D3D12_GPU_VIRTUAL_ADDRESS system_constants);
    // Values written by the slot's last pass; call only after that pass's GPU
    // work is known complete (slot reuse is fence-gated by the caller).
    Probe probe(unsigned slot) const noexcept;
    unsigned width() const noexcept { return width_; }
    unsigned height() const noexcept { return height_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> probe_, readback_;
    const std::uint32_t *mapped_{};
    std::array<bool, slots> written_{};
    unsigned width_{}, height_{}, step_{};
};

}  // namespace mhw::frame_generation

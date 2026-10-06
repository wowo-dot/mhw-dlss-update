#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

// HUD-less colour for DLSS-G: the HUD-free final scene target T (linear
// R11G11B10_FLOAT, before any UI draw) through MHW's screen transform
// (screen_transform.hpp), written to an FG-owned R8G8B8A8_UNORM slot texture.
//
// The brightness parameter lives in the game's CBSystem constant buffer, which
// is bound only to the screen write (after the UI, at the end of its list). A
// one-thread capture pass, recorded as a list's last commands, reads the CBV
// bound there and keeps the last valid value in an FG-owned state buffer; the
// transform reads that state (so it uses the previous screen write's value).
// A per-slot probe records the state the transform actually used.
namespace mhw::frame_generation {

class HudlessPass {
public:
    static constexpr unsigned slots = 3;
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    enum class Shader { transform, capture };
    struct Probe {
        float gamma_ex{}, gamma{};    // Last valid capture (0 before the first)
        bool parameters_ok{}, valid{};  // A valid capture was used; probe written
        std::uint32_t attempts{}, matches{};  // Captures executed / accepted, lifetime
        float last_gamma{}, last_gamma_ex{};  // Raw values of the last attempt
    };
    // A shader as the constructor builds it; throws with the compiler's
    // message. No device needed (offline tests use it).
    static Microsoft::WRL::ComPtr<ID3DBlob> compile(Shader shader);
    // Throws on any creation failure; the caller then runs without HUD-less.
    HudlessPass(ID3D12Device *device, unsigned width, unsigned height);
    // Records on `list` with the caller's bindings already saved. Transitions
    // `scene` RENDER_TARGET -> NON_PIXEL_SHADER_RESOURCE -> RENDER_TARGET and
    // `target` from/to `target_state`. Changes compute root, PSO and heaps.
    void record(ID3D12GraphicsCommandList *list, unsigned slot, ID3D12Resource *scene, ID3D12Resource *target,
                D3D12_RESOURCE_STATES target_state);
    // Records the capture of a candidate CBSystem address. Changes the compute
    // root and PSO (no heaps), so only as the list's last commands.
    void capture(ID3D12GraphicsCommandList *list, D3D12_GPU_VIRTUAL_ADDRESS system_constants);
    // Values written by the slot's last pass; call only after that pass's GPU
    // work is known complete (slot reuse is fence-gated by the caller).
    Probe probe(unsigned slot) const noexcept;
    unsigned width() const noexcept { return width_; }
    unsigned height() const noexcept { return height_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_, capture_root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_, capture_pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> state_, probe_, readback_;
    const std::uint32_t *mapped_{};
    std::array<bool, slots> written_{};
    unsigned width_{}, height_{}, step_{};
};

}  // namespace mhw::frame_generation

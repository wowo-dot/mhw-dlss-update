#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

// HUD-less colour for DLSS-G: the HUD-free final scene target T (linear
// R11G11B10_FLOAT, before any UI draw) through MHW's screen transform
// (screen_transform.hpp), written to an FG-owned slot texture in the back
// buffer's colour space: R8G8B8A8_UNORM through the SDR brightness curve and
// sRGB encode, or (v0.24.8) R10G10B10A2_UNORM through the HDR10 PQ encode.
//
// The SDR brightness lives in the game's CBSystem constant buffer, bound only
// to the screen write (after the UI, at the end of its list). The HDR
// parameters live in CBRenderFrame, bound there too. A one-thread capture
// pass, recorded as a list's last commands, reads a candidate CBV and keeps
// the last value whose layout checks in an FG-owned state buffer; the
// transform reads that state (so it uses the previous screen write's value).
// A per-slot probe records the state the transform actually used.
namespace mhw::frame_generation {

class HudlessPass {
public:
    static constexpr unsigned slots = 3;
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;        // SDR
    static constexpr DXGI_FORMAT hdr_format = DXGI_FORMAT_R10G10B10A2_UNORM; // HDR10
    enum class Shader { transform, capture, transform_hdr, capture_hdr };
    struct Probe {
        float gamma_ex{}, gamma{};             // Last valid SDR capture (0 before the first)
        bool parameters_ok{}, valid{};         // A valid SDR capture was used; probe written
        std::uint32_t attempts{}, matches{};   // SDR captures executed / accepted, lifetime
        float last_gamma{}, last_gamma_ex{};   // Raw values of the last SDR attempt
        std::array<std::uint32_t, 4> region{}; // CBSystem iRegion of the last valid capture
        // HDR (v0.24.8): the transform ran in HDR; a valid capture was used
        // (else the shader's defaults); the parameters it used.
        bool hdr{}, hdr_ok{}, gui_gamma{};
        float white{}, ratio{}, hdr_gamma{};
        std::uint32_t hdr_attempts{}, hdr_matches{}, hdr_parameter{}; // lifetime; root parameter of the last match
        float last_white{}, last_ratio{}, last_hdr_gamma{};           // raw values of the last HDR attempt
        std::uint32_t last_gui{};
    };
    // A shader as the constructor builds it; throws with the compiler's
    // message. No device needed (offline tests use it).
    static Microsoft::WRL::ComPtr<ID3DBlob> compile(Shader shader);
    // Throws on any creation failure; the caller then runs without HUD-less.
    // HDR needs typed UAV stores to R10G10B10A2_UNORM (hdr_supported()).
    HudlessPass(ID3D12Device *device, unsigned width, unsigned height);
    // Records on `list` with the caller's bindings already saved. Transitions
    // `scene` from `scene_state` to NON_PIXEL_SHADER_RESOURCE and back: the
    // final draw's RENDER_TARGET, or COPY_DEST after the native final copy at
    // render scale 1.0 (DLAA). `target` goes from/to `target_state`; its
    // format (format or hdr_format) selects the transform. Changes compute
    // root, PSO and heaps.
    void record(ID3D12GraphicsCommandList *list, unsigned slot, ID3D12Resource *scene, ID3D12Resource *target,
                D3D12_RESOURCE_STATES target_state,
                D3D12_RESOURCE_STATES scene_state = D3D12_RESOURCE_STATE_RENDER_TARGET);
    // Records the capture of a candidate CBSystem address. Changes the compute
    // root and PSO (no heaps), so only as the list's last commands.
    void capture(ID3D12GraphicsCommandList *list, D3D12_GPU_VIRTUAL_ADDRESS system_constants);
    // The same for a candidate CBRenderFrame address, bound at graphics root
    // parameter `parameter` (recorded with a match).
    void capture_hdr(ID3D12GraphicsCommandList *list, D3D12_GPU_VIRTUAL_ADDRESS frame_constants, unsigned parameter);
    // Values written by the slot's last pass; call only after that pass's GPU
    // work is known complete (slot reuse is fence-gated by the caller).
    Probe probe(unsigned slot) const noexcept;
    unsigned width() const noexcept { return width_; }
    unsigned height() const noexcept { return height_; }
    bool hdr_supported() const noexcept { return hdr_supported_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_, capture_root_, capture_hdr_root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_, hdr_pipeline_, capture_pipeline_, capture_hdr_pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> state_, probe_, readback_;
    const std::uint32_t *mapped_{};
    std::array<bool, slots> written_{};
    unsigned width_{}, height_{}, step_{};
    bool hdr_supported_{};
};

} // namespace mhw::frame_generation

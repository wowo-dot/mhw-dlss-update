#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include "sharpen_config.hpp"

// NVSharpen on the final scene target T (0.32.0, docs/research/nvsharpen.md):
// T (R11G11B10_FLOAT, linear, after the grade, before the UI) is read with an
// encode on load (sRGB, or BT.2020 PQ at the game's white level for HDR10),
// sharpened by NIS's NVSharpen (third_party/nis) into a scratch image of T's
// size and format with the matching decode on store, and copied back.
namespace mhw::sharpen {

class SharpenPass {
public:
    // Scene targets whose descriptors are kept between resets (the game may
    // alternate between attachments).
    static constexpr unsigned cached_scenes = 4;
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R11G11B10_FLOAT;
    enum class Result { recorded, layout, scratch, cache_full };
    // A shader as the constructor builds it (SDR or HDR10); throws with the
    // compiler's message. No device needed (offline tests use it).
    static Microsoft::WRL::ComPtr<ID3DBlob> compile(bool hdr);
    // Throws on any creation failure (typed UAV stores to `format` included);
    // the caller then runs without sharpening.
    explicit SharpenPass(ID3D12Device *device);
    // Records on `list` with the caller's bindings already saved. `scene` goes
    // from `scene_state` to NON_PIXEL_SHADER_RESOURCE, then COPY_DEST for the
    // copy back, and ends in `scene_state`. Changes the compute root, PSO and
    // heaps. Anything but `recorded` records nothing.
    Result record(ID3D12GraphicsCommandList *list, ID3D12Resource *scene, D3D12_RESOURCE_STATES scene_state,
                  const Constants &constants, bool hdr);
    // Only when no recorded work can still run (the runtime's drained
    // rebuild): forgets the scenes and the scratch image.
    void reset() noexcept;

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 2> pipelines_; // SDR, HDR10
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> scratch_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, cached_scenes> scenes_;
    unsigned step_{};
};

} // namespace mhw::sharpen

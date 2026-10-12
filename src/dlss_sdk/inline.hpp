#pragma once
#include "live.hpp"
#include "geometry.hpp"
#include "../temporal_present/bindings.hpp"
#include <memory>

namespace mhw::dlss_sdk {
// Deterministic fault points for the owned GPU fixture; runtime uses none.
enum class RecordFault {none,after_conversion,evaluate_result,after_handoff};
struct RecordAttempt {bool commands{},bindings_restored{},native_output_written{},success{};};
// One selected frame only. Prepare on the worker before native hooks open.
// Raw inputs are private R11G11B10/R32_FLOAT/R32_UINT COPY_DEST textures.
// record() follows raw input copies in the same DIRECT-list generation. Native
// TAA may precede it (legacy caller) or be omitted on successful replacement.
// Failure restores native bindings; partial work touches private resources only
// until handoff, which completes its native-output UAV transitions without a
// throwing operation. A same-size caller can recover by copying current color.
// Upscaling callers need their own full-output recovery/routing contract; a raw
// copy of the active rectangle is insufficient. Recovery policy belongs to the
// renderer controller; this class never chooses or invokes native TAA/FXAA.
// The caller proves that the completion fence covers that exact generation.
class InlineFrame {
public:
    InlineFrame(ID3D12Device *,unsigned,unsigned,const std::array<ID3D12Resource *,3> &,
                const std::filesystem::path &,const BorrowedInitialization &,bool continuous=false,bool caller_retained=false);
    // captures=false skips the five diagnostic readback buffers (about 450 MB
    // at 4K); record_continuous() then refuses capture=true.
    InlineFrame(ID3D12Device *,Geometry,const std::array<ID3D12Resource *,3> &,
                const std::filesystem::path &,const BorrowedInitialization &,bool continuous=false,bool caller_retained=false,
                bool captures=true);
    ~InlineFrame();
    InlineFrame(const InlineFrame &)=delete;
    InlineFrame &operator=(const InlineFrame &)=delete;
    bool record(ID3D12GraphicsCommandList *,ID3D12Resource *native_output,
                const temporal_present::Bindings &,HistoryFrame) noexcept;
    // Streaming caller serializes recordings AND their GPU execution order.
    // Private descriptors/resources never change; no per-frame allocation or
    // readback is needed. All native recording generations must retire before
    // finish(), and its fence must cover every injected submission.
    // With caller_retained, the caller also pins every native output/list until
    // successful Reset AND GPU completion; only numeric identities remain here.
    bool record_continuous(ID3D12GraphicsCommandList *,ID3D12Resource *,
                const temporal_present::Bindings &,HistoryFrame,bool reset,float frame_ms,bool capture=false,RecordFault=RecordFault::none) noexcept;
    RecordAttempt last_attempt() const noexcept;
    Geometry geometry() const noexcept;
    ID3D12Resource *raw_color() const noexcept;
    // Borrowed private images after a successful record: converted inputs in
    // NON_PIXEL_SHADER_RESOURCE, SDK/packed output in COPY_SOURCE. Caller must
    // serialize GPU use and restore these states after diagnostic copies.
    std::array<ID3D12Resource *,5> private_images() const noexcept;
    void finish(ID3D12Fence *,std::uint64_t,bool all_recordings_retired);
    // Live rebuild (continuous_dlaa::change_mode): releases the SDK feature
    // and parameters, with or without a successful evaluation, once the
    // caller proved every recording that used this frame complete: `fence`
    // reached `value`, signalled after them on their queue. The destructor
    // then frees the images instead of keeping them for the process.
    void retire(ID3D12Fence *,std::uint64_t value);
    bool released() const noexcept;
    // Converted color/depth/motion, SDK RGBA32 output, native packed output.
    std::array<std::vector<std::byte>,5> collect(ID3D12Fence *,std::uint64_t,bool recording_retired);
    void write(std::ostream &) const;
    bool recorded() const noexcept;
private:
    bool record_frame(ID3D12GraphicsCommandList *,ID3D12Resource *,const temporal_present::Bindings &,
                      HistoryFrame,bool,float,bool,RecordFault=RecordFault::none) noexcept;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

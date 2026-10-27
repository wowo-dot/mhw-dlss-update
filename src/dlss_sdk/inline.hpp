#pragma once
#include "initialization.hpp"
#include "history.hpp"
#include "geometry.hpp"
#include "../temporal_present/bindings.hpp"
#include <array>
#include <iosfwd>
#include <memory>

namespace mhw::dlss_sdk {
struct RecordAttempt {
    bool commands{}, bindings_restored{}, native_output_written{}, success{};
};
// DLSS in place of the game's TAA: the input converter and the SDK feature for
// one geometry, recorded into the game's list once a frame. Prepare on the
// worker before native hooks open. Raw inputs are private
// R11G11B10/R32_FLOAT/R32_UINT COPY_DEST textures; record() follows their
// copies in the same DIRECT-list generation.
// Failure restores native bindings; partial work touches private resources only
// until handoff, which completes its native-output UAV transitions without a
// throwing operation. The caller recovers the full output after a failure
// (scaled_temporal::record_resolve); this class never chooses or invokes
// native TAA/FXAA.
class InlineFrame {
public:
    // byte_limit: the most its images may take, measured exactly as they are
    // created (the runtime passes the room left in the video memory budget,
    // gpu_memory).
    InlineFrame(ID3D12Device *, Geometry, const std::array<ID3D12Resource *, 3> &, const std::filesystem::path &,
                const BorrowedInitialization &, std::uint64_t byte_limit);
    ~InlineFrame();
    InlineFrame(const InlineFrame &) = delete;
    InlineFrame &operator=(const InlineFrame &) = delete;
    // The caller serializes recordings AND their GPU execution order, and pins
    // every native output/list until successful Reset AND GPU completion; only
    // numeric identities remain here. Private descriptors/resources never
    // change; no per-frame allocation or readback is needed.
    bool record(ID3D12GraphicsCommandList *, ID3D12Resource *native_output, const temporal_present::Bindings &,
                HistoryFrame, bool reset, float frame_ms) noexcept;
    RecordAttempt last_attempt() const noexcept;
    Geometry geometry() const noexcept;
    // The NGX render preset it was created with (options.hpp's render_preset).
    unsigned preset() const noexcept;
    ID3D12Resource *raw_color() const noexcept;
    // Borrowed private images after a successful record: converted inputs in
    // NON_PIXEL_SHADER_RESOURCE, SDK/packed output in COPY_SOURCE. Caller must
    // serialize GPU use and restore these states after its copies.
    std::array<ID3D12Resource *, 5> private_images() const noexcept;
    // Live rebuild (continuous_dlaa::change_mode): releases the SDK feature
    // and parameters, with or without a successful evaluation, once the
    // caller proved every recording that used this frame complete: `fence`
    // reached `value`, signalled after them on their queue. The destructor
    // then frees the images instead of keeping them for the process.
    void retire(ID3D12Fence *, std::uint64_t value);
    // Bytes of the images it created, measured with GetResourceAllocationInfo.
    std::uint64_t allocation_bytes() const noexcept;
    void write(std::ostream &) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mhw::dlss_sdk

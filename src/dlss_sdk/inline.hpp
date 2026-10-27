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
// copies in the same DIRECT-list generation. DLSS reads the colour copy and
// the converted depth and motion, and writes the game's TAA output itself
// (0.32.0: no private output, packing or copy back).
// Failure restores native bindings and the copies' states. The caller
// recovers the full output after a failure (scaled_temporal::record_resolve);
// this class never chooses or invokes native TAA/FXAA.
class InlineFrame {
public:
    // Frame sets of converted depth and motion: 1, or max_sets while frame
    // generation can run, so it reads frame f's set (f % max_sets, the
    // staging key) without copying it first (0.32.0, dlss-own-work.md).
    static constexpr unsigned max_sets = 3;
    // The converted images' state after a record, until their set's next one:
    // DLSS reads them, frame generation copies them.
    static constexpr auto read_state =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_COPY_SOURCE;
    // byte_limit: the most its images may take, measured exactly as they are
    // created (the runtime passes the room left in the video memory budget,
    // gpu_memory).
    InlineFrame(ID3D12Device *, Geometry, const std::array<ID3D12Resource *, 3> &, const std::filesystem::path &,
                const BorrowedInitialization &, std::uint64_t byte_limit, unsigned frame_sets = 1);
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
    // Borrowed private images of a frame set: the converted depth (R32_FLOAT)
    // and motion (R16G16_FLOAT) at the render size, in read_state once written
    // (frame f writes set f % sets()). Null past sets(). Caller must
    // serialize GPU use and never change their state.
    unsigned sets() const noexcept;
    std::array<ID3D12Resource *, 2> private_images(unsigned set) const noexcept;
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

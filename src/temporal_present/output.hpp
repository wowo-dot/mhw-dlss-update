#pragma once
#include <d3d12.h>
#include <cstdint>
#include <memory>

namespace mhw::temporal_present {
// Spatial recovery reads only this top-left active region of a packed HDR
// allocation. It has no temporal history and is not an anti-aliasing fallback.
struct SpatialSource {
    unsigned allocation_width{}, allocation_height{}, width{}, height{};
    bool operator==(const SpatialSource &) const = default;
};
// Prepared on a worker; one recording on a DIRECT list. Conversion writes only
// a private R11G11B10 texture, followed by a copy into the native UAV. No native
// descriptors are modified. The caller must restore compute/graphics bindings
// after record() and retain all objects through GPU completion/list retirement.
class Output {
public:
    // caller_retained is continuous-only: caller owns every destination/list
    // through both its successful Reset and GPU completion. No native object
    // references are retained here in that mode (private objects still are).
    Output(ID3D12Device *, unsigned width, unsigned height, bool continuous=false,bool caller_retained=false);
    Output(ID3D12Device *, SpatialSource, unsigned output_width, unsigned output_height,
           bool continuous=false, bool caller_retained=false);
    ~Output();
    Output(const Output &) = delete;
    Output &operator=(const Output &) = delete;
    // Checks and creates private views before recording any command. Both
    // resources must be distinct, same-device, full-size single-mip textures:
    // RGBA32_FLOAT source in COPY_SOURCE, R11G11B10_FLOAT destination in UAV.
    // Spatial construction instead requires packed R11G11B10 source with the
    // specified allocation dimensions, and samples only its active region.
    void bind(ID3D12Resource *source, ID3D12Resource *destination);
    // Reserves bounded native references and validates the list without issuing
    // commands. A renderer can reserve recovery before recording SDK work.
    void prepare_record(ID3D12GraphicsCommandList *);
    // Source state is restored; destination starts/ends UAV. State knowledge
    // belongs to the caller. All validation precedes the first command.
    void record(ID3D12GraphicsCommandList *, D3D12_RESOURCE_STATES source_state=D3D12_RESOURCE_STATE_COPY_SOURCE);
    bool spatial_contract(SpatialSource, unsigned output_width, unsigned output_height) const noexcept;
    // Call only after the containing native recording completed AND retired.
    // Without this proof the destructor retains references for process lifetime.
    void retire(ID3D12Fence *, std::uint64_t completed_value, bool recording_retired);
    bool recorded() const noexcept;
    bool retired() const noexcept;
    ID3D12Resource *packed() const noexcept;
private:
    Output(ID3D12Device *, unsigned, unsigned, bool, bool, const SpatialSource *);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

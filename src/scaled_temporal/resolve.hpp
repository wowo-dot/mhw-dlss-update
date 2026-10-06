#pragma once
#include "../continuous_dlaa/resolve.hpp"
#include "../temporal_present/output.hpp"

namespace mhw::scaled_temporal {
// Prepared packed-HDR spatial recovery must use this exact SDK geometry and
// private raw color. Reserve it before claiming the native resolve. A successful
// return fills the complete output and restores source/UAV states and bindings.
// This is a GPU transaction, not permission to enlarge native downstream draws:
// the renderer separately qualifies its filter/transfer/viewport routing.
// The caller serializes GPU execution, owns all native recordings/resources,
// and retires both pass and recovery after Reset AND GPU completion. Raster
// jitter cannot be undone by spatial recovery. No native AA is invoked here.
continuous_dlaa::ResolveResult record_resolve(dlss_sdk::InlineFrame &,
    temporal_present::Output &recovery, ID3D12GraphicsCommandList *, ID3D12Resource *output,
    const temporal_present::Bindings &, dlss_sdk::HistoryFrame, bool reset, float frame_ms,
    bool evaluate=true, bool capture=false, dlss_sdk::RecordFault=dlss_sdk::RecordFault::none) noexcept;
}

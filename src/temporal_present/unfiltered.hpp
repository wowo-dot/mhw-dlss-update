#pragma once
#include <d3d12.h>

namespace mhw::temporal_present {
// Read-only validation for reserving an unfiltered recovery route before SDK
// commands are recorded. The caller separately proves current resource states.
bool can_copy_unfiltered(ID3D12GraphicsCommandList *, ID3D12Resource *source, ID3D12Resource *destination,
                         unsigned width, unsigned height, D3D12_RESOURCE_STATES source_state) noexcept;
// Full-resolution AA-Off handoff. No shader, history, or binding changes.
// Caller proves the listed states and owns both resources/list through Reset
// AND GPU completion. This does not undo jitter already used to rasterize.
// All rejection occurs before the first command; false leaves the list alone.
bool copy_unfiltered(ID3D12GraphicsCommandList *, ID3D12Resource *source, ID3D12Resource *destination, unsigned width,
                     unsigned height, D3D12_RESOURCE_STATES source_state) noexcept;
} // namespace mhw::temporal_present

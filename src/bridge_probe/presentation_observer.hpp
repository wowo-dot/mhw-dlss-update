#pragma once
#include <dxgi1_2.h>
#include <cstdint>

namespace mhw::bridge_probe {
struct PresentationCreation {
  DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
  UINT width{}, height{}, buffers{}, flags{};
  LUID adapter_luid{};
  unsigned observed{}, matching{};
  bool installed{}, fault_latched{}, overflow{}, in_flight{}, available{};
};
// Install once after MinHook initialization and exact environment admission.
// Two system-DXGI factory hooks observe at most eight successful creations.
// Hooks stay process-lived; all COM references are released inside callbacks.
bool install_presentation_observer() noexcept;
// Select only one fully described chain created for this adapter and a window
// owned by this process. This is creation metadata, not current Present state.
PresentationCreation presentation_creation(LUID adapter) noexcept;
} // namespace mhw::bridge_probe

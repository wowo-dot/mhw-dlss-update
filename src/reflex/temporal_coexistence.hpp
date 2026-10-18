#pragma once
#include <windows.h>
#include <filesystem>
#include <iosfwd>
#include <cstdint>
struct ID3D12Device;

namespace mhw::reflex::temporal {
// Only the presentation worker calls these functions. Both SDKs retain the
// same native device; NGX ownership remains process-lived as in the accepted
// temporal renderer. No second worker, arm event, or SDK shutdown is introduced.
const char *version() noexcept;
// The NGX runtime folder: `ngx_runtime` when given (the plugin), else the output
// folder's sdk-runtime-dir.txt (research runs).
void prepare(HMODULE, const std::filesystem::path &, std::uintptr_t, const std::filesystem::path &ngx_runtime = {});
// The continuous runtime would install now (no side effects; see cd::ready).
bool ready(std::string &reason);
void arm(ID3D12Device *);
bool healthy() noexcept;
// The continuous runtime's scene and window sizes (see continuous_dlaa).
bool scene_extent(unsigned &scene_width, unsigned &scene_height, unsigned &window_width,
                  unsigned &window_height) noexcept;
// DLSS's render size (see continuous_dlaa).
bool render_extent(unsigned &width, unsigned &height) noexcept;
void sample();
// The runtime's counters (see cd::write_summary), or null before arming.
void summary(std::ostream &);
// Writes the runtime state to the input log and status file without requiring
// health (for the record of why the runtime became unhealthy). No throw.
void fault_snapshot() noexcept;
bool finish() noexcept;
void receipt(std::ostream &, const char *phase, ID3D12Device *);
} // namespace mhw::reflex::temporal

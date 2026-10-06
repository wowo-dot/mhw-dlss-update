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
const char* version() noexcept;
void prepare(HMODULE, const std::filesystem::path&, std::uintptr_t);
void arm(ID3D12Device*);
bool healthy() noexcept;
void sample();
// Writes the runtime state to the input log and status file without requiring
// health (for the record of why the runtime became unhealthy). No throw.
void fault_snapshot() noexcept;
bool finish() noexcept;
void receipt(std::ostream&, const char* phase, ID3D12Device*);
}

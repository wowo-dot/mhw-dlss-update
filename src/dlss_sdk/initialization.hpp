#pragma once
#include <d3d12.h>
#include <filesystem>

namespace mhw::dlss_sdk {
// Supplied only by an owner retaining successful initialization on this device
// (NGX start-up). Validation returns its pinned provider, or null; it never
// initializes NGX.
struct BorrowedInitialization {
    HMODULE (*validate)(ID3D12Device *,const std::filesystem::path &) noexcept{};
};
}

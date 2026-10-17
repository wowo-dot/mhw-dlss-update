#include "silhouette_shader.hpp"
#include "screen_shader.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <climits>
#include <stdexcept>

namespace mhw::scaled_temporal {
namespace {
void require(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
struct Allowed {
    ShaderHash hash;
    unsigned bytes;
};
// Identities only, never native shader code.
constexpr Allowed allowed[]{
#include "silhouette_allowlist.inc"
};
} // namespace
bool shader_sha256(std::span<const std::byte> input, ShaderHash &hash) noexcept {
    hash = {};
    return input.size() <= ULONG_MAX &&
           BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                      reinterpret_cast<PUCHAR>(const_cast<std::byte *>(input.data())), static_cast<ULONG>(input.size()),
                      hash.data(), static_cast<ULONG>(hash.size())) >= 0;
}
bool known_silhouette_shader(std::span<const std::byte> b) noexcept {
    if (b.size() < 52 || b.size() > 131072) return false;
    ShaderHash hash;
    if (!shader_sha256(b, hash)) return false;
    for (const auto &a : allowed)
        if (b.size() == a.bytes && hash == a.hash) return true;
    return false;
}
std::size_t known_silhouette_shader_count() noexcept {
    return std::size(allowed);
}
std::vector<std::byte> adapt_silhouette_shader(std::span<const std::byte> native, FilterGeometry g) {
    require(valid_filter_geometry(g), "silhouette geometry");
    ScreenShaderEdits edits;
    auto adapted = make_screen_shader(native, g, UINT_MAX, edits);
    // Rule 1 only: these shaders read no scene; at least the mask load reads the position.
    require(edits.position_reads > 0 && !edits.scene_samples && !edits.scene_loads, "silhouette position reads");
    return adapted;
}
std::vector<std::byte> make_silhouette_shader(std::span<const std::byte> native, FilterGeometry g) {
    require(known_silhouette_shader(native), "unrecognized native silhouette shader");
    return adapt_silhouette_shader(native, g);
}
} // namespace mhw::scaled_temporal

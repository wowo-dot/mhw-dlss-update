#include "recorder.hpp"
#include <cstring>

namespace mhw::projection_binding {
bool dxbc_valid(const std::byte *bytes, unsigned size) noexcept {
    if (!bytes || size < 32 || size > max_shader || std::memcmp(bytes, "DXBC", 4)) return false;
    const auto word = [&](unsigned off) {
        unsigned v{};
        std::memcpy(&v, bytes + off, 4);
        return v;
    };
    if (word(20) != 1 || word(24) != size) return false;
    const auto count = word(28);
    if (!count || count > 64 || count > (size - 32) / 4) return false;
    const auto header = 32 + count * 4;
    for (unsigned i = 0; i < count; ++i) {
        const auto at = word(32 + 4 * i);
        if (at < header || at > size - 8 || word(at + 4) > size - at - 8) return false;
        const auto end = at + 8 + word(at + 4);
        for (unsigned j = 0; j < i; ++j) {
            const auto prior = word(32 + 4 * j), last = prior + 8 + word(prior + 4);
            if (at < last && prior < end) return false;
        }
    }
    return true;
}
} // namespace mhw::projection_binding

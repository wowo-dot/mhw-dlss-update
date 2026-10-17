#include "projection_snapshot.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace mhw::bridge_probe {
namespace {
struct Reader {
    ScalarReader read;
    void *opaque;
    unsigned count{};
    bool ok{true};
    template <class T> T get(std::uintptr_t at, std::uintptr_t offset = 0) noexcept {
        T value{};
        if (!ok) return value;
        if (!at || at > (std::numeric_limits<std::uintptr_t>::max)() - offset || ++count > 192) {
            ok = false;
            return value;
        }
        ok = read &&
             read(opaque, at + offset, std::as_writable_bytes(std::span(&value, 1))) == ScalarReadResult::success;
        return value;
    }
    std::uintptr_t ptr(std::uintptr_t at, std::uintptr_t offset = 0) noexcept {
        return get<std::uintptr_t>(at, offset);
    }
};
float value(std::uint32_t bits) noexcept {
    return std::bit_cast<float>(bits);
}
bool matrix_parameter(unsigned id, unsigned words) noexcept {
    const auto offset = (id >> 12) & 0x3ffU;
    return id != 0xffffffffU && ((id >> 8) & 15U) == 15U && offset + 16 <= words && offset + 16 <= 1024;
}
bool anchor_matches(Reader &r, std::uintptr_t base, const ProjectionAnchor &a) noexcept {
    return r.ptr(base, 0x51c4480) == a.renderer && r.ptr(base, 0x51c4368) == a.table &&
           r.ptr(a.renderer, 0x1f450) == a.view && r.get<std::uint64_t>(a.renderer, 0x1f210) == a.frame &&
           r.get<std::uint64_t>(a.renderer, 0x1f208) == a.epoch && r.get<unsigned>(a.renderer, 0x1f200) == a.bank &&
           r.get<unsigned char>(a.renderer, 0x93) == a.async_mode;
}
} // namespace
bool projection_relation_matches(const ProjectionSnapshot &s) noexcept {
    if (!s.valid) return false;
    const double w = value(s.viewport_bits[0]), h = value(s.viewport_bits[1]);
    const double x = value(s.jitter_bits[0]), y = value(s.jitter_bits[1]);
    if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0 || !std::isfinite(x) || !std::isfinite(y))
        return false;
    bool homogeneous{};
    for (unsigned row = 0; row < 4; ++row) {
        const double factor = value(s.unjittered_bits[4 * row + 3]);
        homogeneous = homogeneous || factor != 0;
        for (unsigned col = 0; col < 4; ++col) {
            const double p = value(s.projection_bits[4 * row + col]), n = value(s.unjittered_bits[4 * row + col]);
            const double delta = col == 0 ? factor * x / w : col == 1 ? factor * y / h : 0;
            if (!std::isfinite(p) || !std::isfinite(n) || !std::isfinite(delta)) return false;
            // Eight float epsilons at the actual coefficient scale; tiny offsets
            // are not accepted just because they fall below an absolute 1e-6.
            const double scale = (std::max)({1e-6, std::abs(p), std::abs(n), std::abs(delta)});
            if (std::abs((p - n) - delta) > 8 * std::numeric_limits<float>::epsilon() * scale) return false;
        }
    }
    return homogeneous;
}
ProjectionSnapshot capture_projection_snapshot(std::uintptr_t base, const ProjectionAnchor &a, ScalarReader read,
                                               void *opaque) noexcept {
    Reader r{read, opaque};
    ProjectionSnapshot s{};
    s.attempted = true;
    s.checkpoint = 1;
    if (!a.renderer || !a.context || !a.view || !a.table || a.bank > 1 || a.async_mode > 1 ||
        a.renderer > (std::numeric_limits<std::uintptr_t>::max)() - 0x710 || a.context != a.renderer + 0x710) {
        s.rejection = ProjectionReject::anchor;
        return s;
    }
    bool stable = anchor_matches(r, base, a);
    s.manager = r.ptr(base, 0x51c4400);
    s.vtable = r.ptr(s.manager);
    s.vtable_rva = s.vtable >= base ? s.vtable - base : 0;
    s.epoch = r.get<unsigned>(s.manager, 0x40);
    s.buffer_id = r.get<unsigned>(base, 0x4be75f4);
    s.projection_id = r.get<unsigned>(base, 0x4be7604);
    s.unjittered_id = r.get<unsigned>(base, 0x4be763c);
    s.metadata_table = r.ptr(a.context, 0x2b18);
    s.checkpoint = 2;
    // The pinned derived constructor calls the base constructor at the same
    // object address, then replaces its vtable. Both retain projection slot 0x48.
    if (!r.ok)
        s.rejection = ProjectionReject::read;
    else if (!stable)
        s.rejection = ProjectionReject::source_changed;
    else if (s.vtable_rva != 0x350d188 && s.vtable_rva != 0x33f82a8)
        s.rejection = ProjectionReject::camera_type;
    else if (s.epoch != static_cast<unsigned>(a.epoch))
        s.rejection = ProjectionReject::camera_epoch;
    else if (s.buffer_id == 0xffffffffU)
        s.rejection = ProjectionReject::buffer_unavailable;
    if (s.rejection != ProjectionReject::none) {
        s.reads = r.count;
        return s;
    }
    const auto slot = s.buffer_id & 0xfffU;
    s.metadata = r.ptr(s.metadata_table, 8ULL * slot);
    s.words = r.get<unsigned>(s.metadata, 0x10);
    s.tagged_constants = r.ptr(a.context, 0x710 + 8ULL * slot);
    s.constants = s.tagged_constants & ~std::uintptr_t{15};
    s.checkpoint = 3;
    const auto proj_offset = (s.projection_id >> 12) & 0x3ffU, plain_offset = (s.unjittered_id >> 12) & 0x3ffU;
    if (!r.ok || !s.constants || s.words > 4096 || !matrix_parameter(s.projection_id, s.words) ||
        !matrix_parameter(s.unjittered_id, s.words) ||
        !(proj_offset + 16 <= plain_offset || plain_offset + 16 <= proj_offset)) {
        s.rejection = r.ok ? ProjectionReject::layout : ProjectionReject::read;
        s.reads = r.count;
        return s;
    }
    for (unsigned i = 0; i < 2; ++i) {
        s.jitter_bits[i] = r.get<unsigned>(s.manager, 0x38 + 4 * i);
        s.table20_bits[i] = r.get<unsigned>(a.table, 0xe6a0 + 8 * (a.frame % 20) + 4 * i);
        s.viewport_bits[i] = r.get<unsigned>(a.context, 0x230 + 4 * i);
    }
    for (unsigned i = 0; i < 16; ++i) {
        s.projection_bits[i] = r.get<unsigned>(s.constants, 4ULL * (proj_offset + i));
        s.unjittered_bits[i] = r.get<unsigned>(s.constants, 4ULL * (plain_offset + i));
    }
    s.checkpoint = 4;
    for (unsigned i = 0; i < 16; ++i) {
        stable = (r.get<unsigned>(s.constants, 4ULL * (proj_offset + i)) == s.projection_bits[i]) && stable;
        stable = (r.get<unsigned>(s.constants, 4ULL * (plain_offset + i)) == s.unjittered_bits[i]) && stable;
        stable = std::isfinite(value(s.projection_bits[i])) && std::isfinite(value(s.unjittered_bits[i])) && stable;
    }
    for (unsigned i = 0; i < 2; ++i) {
        stable = (r.get<unsigned>(s.manager, 0x38 + 4 * i) == s.jitter_bits[i] &&
                  r.get<unsigned>(a.table, 0xe6a0 + 8 * (a.frame % 20) + 4 * i) == s.table20_bits[i] &&
                  r.get<unsigned>(a.context, 0x230 + 4 * i) == s.viewport_bits[i]) &&
                 stable;
        stable = std::isfinite(value(s.jitter_bits[i])) && std::isfinite(value(s.table20_bits[i])) &&
                 std::isfinite(value(s.viewport_bits[i])) && value(s.viewport_bits[i]) > 0 && stable;
    }
    stable =
        (r.ptr(a.context, 0x710 + 8ULL * slot) == s.tagged_constants && r.ptr(a.context, 0x2b18) == s.metadata_table &&
         r.ptr(s.metadata_table, 8ULL * slot) == s.metadata && r.get<unsigned>(s.metadata, 0x10) == s.words) &&
        stable;
    stable = (r.get<unsigned>(base, 0x4be75f4) == s.buffer_id && r.get<unsigned>(base, 0x4be7604) == s.projection_id &&
              r.get<unsigned>(base, 0x4be763c) == s.unjittered_id && r.ptr(base, 0x51c4400) == s.manager &&
              r.ptr(s.manager) == s.vtable && r.get<unsigned>(s.manager, 0x40) == s.epoch) &&
             stable;
    stable = anchor_matches(r, base, a) && stable;
    s.reads = r.count;
    s.valid = r.ok && stable;
    if (s.valid) s.checkpoint = 100;
    if (!s.valid) s.rejection = r.ok ? ProjectionReject::changed_or_nonfinite : ProjectionReject::read;
    s.matrix_relation = projection_relation_matches(s);
    return s;
}
} // namespace mhw::bridge_probe

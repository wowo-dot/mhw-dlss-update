#pragma once
#include "../temporal_bindings/shader.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string_view>

namespace mhw::continuous_dlaa {
inline constexpr unsigned native_taa_shader_bytes = 15492;
inline constexpr char native_taa_shader_sha[] = "2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1";
struct NativeShaderVariant {
    unsigned technique{}, bytes{};
    std::string_view sha;
};
inline constexpr std::array<NativeShaderVariant, 2> native_shader_variants{
    {{23086, native_taa_shader_bytes, native_taa_shader_sha},
     {29842, 15284, "e14459733cbbccc47fdaed77ce818062968cf96303f12e28bde7d86a50d6b47c"}}};
inline constexpr unsigned unknown_native_shader = static_cast<unsigned>(native_shader_variants.size());
inline unsigned native_shader_index(unsigned technique, unsigned bytes, bool include_menu = false) noexcept {
    for (unsigned i = 0; i < (include_menu ? unknown_native_shader : 1U); ++i)
        if (native_shader_variants[i].technique == technique && native_shader_variants[i].bytes == bytes) return i;
    return unknown_native_shader;
}
// Bounded independent caches: switching variants must not compare menu bytes
// against a previously verified gameplay shader (or trust technique alone).
struct NativeShaderCache {
    std::array<std::array<std::byte, native_taa_shader_bytes>, native_shader_variants.size()> bytes{};
    std::array<bool, native_shader_variants.size()> verified{};
    std::array<std::uint64_t, native_shader_variants.size()> accepted{};
    template <class Hash> bool matches(unsigned index, std::span<const std::byte> current, Hash hash) {
        if (index >= native_shader_variants.size() || current.size() != native_shader_variants[index].bytes)
            return false;
        if (verified[index]) {
            if (!std::equal(current.begin(), current.end(), bytes[index].begin())) return false;
        } else {
            if (hash(current) != native_shader_variants[index].sha) return false;
            std::copy(current.begin(), current.end(), bytes[index].begin());
            verified[index] = true;
        }
        ++accepted[index];
        return true;
    }
};
// The game's TAA without reprojection (0.30.2, marie-2): CSTemporalAA2's
// _NO_REPROJECTION technique, picked when a scene's data turns
// uTemporalAAFilter2's mReprojection off (layout::taa_filter::reprojection,
// set through uSdlTemporalAA). It reads only the frame and the history (no
// depth, motion or camera), so it can't be a DLSS frame: the game runs its
// own TAA for it, and DLSS takes the next frame of the usual technique. Up to
// 0.30.1 its dispatch was an unknown shader (fault 29) and stopped DLSS for
// the session. Vanilla: ShaderPackage.sdf holds these exact bytes.
inline constexpr NativeShaderVariant no_reprojection_variant{
    26713, 11644, "6b07981dbcf7f589d2157d8a3e032f7395e2501b6ff94a4e98139c74bea7c24f"};
inline bool no_reprojection_shader(unsigned technique, unsigned bytes) noexcept {
    return technique == no_reprojection_variant.technique && bytes == no_reprojection_variant.bytes;
}
// Its bytes (hashed once, then compared) and the frames it took: runs of
// consecutive frames, with the stage id where the last run began (-1 when
// unread) and the first stages seen.
struct NoReprojectionFrames {
    std::array<std::byte, no_reprojection_variant.bytes> bytes{};
    bool verified{};
    std::uint64_t frames{}, runs{}, run_start{}, last_frame{};
    int run_stage{-1};
    std::array<int, 8> stages{};
    unsigned stage_count{};
    template <class Hash> bool matches(std::span<const std::byte> current, Hash hash) {
        if (current.size() != bytes.size()) return false;
        if (verified) return std::equal(current.begin(), current.end(), bytes.begin());
        if (hash(current) != no_reprojection_variant.sha) return false;
        std::copy(current.begin(), current.end(), bytes.begin());
        verified = true;
        return true;
    }
    void note(std::uint64_t frame, int stage) noexcept {
        if (!frames || frame != last_frame + 1) {
            ++runs;
            run_start = frame;
            run_stage = stage;
            if (std::find(stages.begin(), stages.begin() + stage_count, stage) == stages.begin() + stage_count &&
                stage_count < stages.size())
                stages[stage_count++] = stage;
        }
        last_frame = frame;
        ++frames;
    }
    void write(std::ostream &o) const {
        o << "{\"frames\":" << frames << ",\"runs\":" << runs << ",\"run_start\":" << run_start
          << ",\"last_frame\":" << last_frame << ",\"run_stage\":" << run_stage << ",\"stages\":[";
        for (unsigned i = 0; i < stage_count; ++i) o << (i ? "," : "") << stages[i];
        o << "]}";
    }
};
// Structural gate for the exact captured cs_5_0. Callers must ALSO verify its
// bytecode, current bindings/states, full output extent and resource lifetime.
inline bool single_native_output(const temporal_bindings::Snapshot &s, const temporal_bindings::ShaderRecipe &p,
                                 bool include_menu = false) noexcept {
    if (native_shader_index(p.technique, p.size, include_menu) == unknown_native_shader || s.technique != p.technique ||
        !p.owner_count || p.owner_count > p.owners.size() || p.binding_count > p.bindings.size())
        return false;
    constexpr unsigned srv_roles[]{0, 1, 2, 3, 5, 6};
    unsigned mask{};
    for (unsigned i = 0; i < p.binding_count; ++i) {
        const auto w = p.bindings[i].word, kind = w & 15, index = (w >> 4) & 255, slot = w >> 24;
        if (kind > 3) return false;
        if (kind != 2 && kind != 3) continue; // non-texture entries; exact bytecode is separately gated
        if (index >= p.owner_count || !p.bindings[i].view || !p.bindings[i].cpu_descriptor) return false;
        unsigned role{}, bit{};
        if (kind == 3) {
            if (slot) return false;
            role = 4;
            bit = 64;
        } else {
            if (slot >= 6) return false;
            role = srv_roles[slot];
            bit = 1U << slot;
        }
        const auto &owner = p.owners[index];
        const auto &named = s.inputs[role];
        if ((mask & bit) || !named.resource || !named.object || owner.object != named.object ||
            (owner.id & 4095) != (named.id & 4095))
            return false;
        if (kind == 2 && named.resource == s.inputs[4].resource) return false;
        mask |= bit;
    }
    return mask == 127;
}
} // namespace mhw::continuous_dlaa

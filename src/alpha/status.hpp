#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include "capabilities.hpp"

// What the mod is doing, for CMFM's Status page (alpha).
// - The worker and the Astera Loader entry report it (status.cpp keeps it
//   under a short lock).
// - The menu thread takes a snapshot and adds the live values: DLSS health
//   and the frame generation session.
// - The line functions below are pure (tests/alpha_menu_tests.cpp).
namespace mhw::alpha::status {

enum class Loader : unsigned { active, degraded, disabled };  // as reported to Astera Loader
// player_off: DLSS only, the player's Off (the game renders as without the mod).
enum class Stage : unsigned { starting, running, off, player_off };  // DLSS, Reflex
// Frame generation in this session: off in the ini, waiting for the game, armed,
// not possible here, or stopped after it ran.
enum class Generation : unsigned { off, starting, armed, unavailable, stopped };

struct View {
    Loader loader = Loader::active;
    std::string loader_reason;
    std::string dlss_mode = "Performance";  // as players read it
    std::string reflex_mode = "On";         // the mode applied: On, Boost or Off
    Stage dlss = Stage::starting, reflex = Stage::starting;
    std::string dlss_reason, reflex_reason;
    unsigned render_width{}, render_height{}, scene_width{}, scene_height{};
    Generation generation = Generation::off;
    std::string generation_reason;
    // Live values, filled in by the menu.
    bool dlss_healthy = true;
    bool generation_enabled = true;       // the player's frame generation setting
    bool generating{};                    // DLSS-G on at the last Present
    unsigned requested{}, generated{};    // total multiplier asked for; generated frames per rendered frame
    std::uint64_t presented_generated{};  // generated frames shown since the start
    // The mode asked for (0 fixed, 1 Auto, 2 Dynamic) and Dynamic's target (0:
    // display refresh); whether DLSS-G runs Dynamic now and whether the card
    // supports it (0 not known, 1 no, 2 yes); frames shown for the last
    // rendered frame, and Presents in a row with none generated.
    unsigned generation_mode{}, target{}, dynamic_support{}, presented_now{}, single_streak{};
    unsigned display_hz{};  // the game's monitor, 0 if unknown (display_refresh.hpp)
    bool dynamic_applied{}, dynamic_refused{};
    capabilities::Capabilities caps;  // what this card offers (v0.27.5)
};
// Presents in a row without a generated frame before Auto reads as paused
// (single ones happen at camera cuts).
inline constexpr unsigned auto_pause_presents = 30;

// Worker and plugin entry, any thread.
void loader(Loader, std::string_view reason) noexcept;
void dlss(Stage, std::string_view reason = {}) noexcept;
void reflex(Stage, std::string_view reason = {}) noexcept;
void generation(Generation, std::string_view reason = {}) noexcept;
void dlss_sizes(unsigned render_width, unsigned render_height, unsigned scene_width, unsigned scene_height) noexcept;
void dlss_mode(std::string_view name) noexcept;
void reflex_mode(std::string_view name) noexcept;
// v0.27.5: what this card offers, at start (capabilities.hpp), then as learned:
// DLSS (NGX's capability, at arming) and DLSS-G's multiplier limit and
// Dynamic support (its state, once read).
void card_capabilities(const capabilities::Capabilities &) noexcept;
void dlss_available(bool) noexcept;
void generation_limits(unsigned max_multiplier, capabilities::Support dynamic) noexcept;
// The worker stopped on `reason`: what had not started yet is off.
void worker_failed(std::string_view reason) noexcept;
// Menu thread.
View snapshot();

// A reason from the log, for players: known ones get words, the rest point at
// the log. Matches the whole reason or its "reason:code" form.
inline std::string reason_text(std::string_view raw) {
    struct Known {
        std::string_view key, text;
    };
    static constexpr Known known[]{
        {"DLSS-G-support", "this graphics card or driver has no DLSS Frame Generation"},
        {"live-frame-al-hook-refused", "another mod changed the game's frame functions"},
        {"live-frame-al-hooks-missing", "this Astera Loader has no al.hooks"},
        {"live-frame-full-gate", "the game's frame functions differ from the tested game"},
        {"retail-host-identity", "this game version is not supported"},
        {"temporal_unhealthy", "DLSS stopped"},
        {"reflex_frames", "the frame hooks stopped"},
        {"presentation", "presentation stopped"},
        {"scene_rect", "the game's screen layout is not supported"},
        // v0.28.6: frame generation's own reasons (presentation_probe.cpp).
        {"presentation fault", "a presentation error turned it off for this session"},
        {"exclusive fullscreen", "not available in exclusive fullscreen; use borderless or windowed"},
        {"latency waitable", "the game's frame pacing is not as expected"},
        {"switch failed", "it could not be loaded; see the mod's log"},
    };
    for (const auto &k : known)
        if (raw == k.key || (raw.size() > k.key.size() && raw.starts_with(k.key) && raw[k.key.size()] == ':'))
            return std::string(k.text);
    if (raw.empty()) return "see the mod's log";
    return "see the mod's log (" + std::string(raw.substr(0, 60)) + ")";
}

inline std::string with_reason(std::string_view state, std::string_view reason) {
    std::string out(state);
    if (!reason.empty()) out.append(": ").append(reason);
    return out;
}

inline std::string size_text(unsigned width, unsigned height) {
    return std::to_string(width) + "x" + std::to_string(height);
}

inline std::string count_text(std::uint64_t n) {
    auto digits = std::to_string(n);
    for (auto at = digits.size(); at > 3; at -= 3) digits.insert(at - 3, ",");
    return digits;
}

inline std::string mod_line(const View &v) {
    if (v.loader == Loader::disabled) return with_reason("Disabled", v.loader_reason);
    if (v.loader == Loader::degraded) return with_reason("Degraded", v.loader_reason);
    if (v.dlss == Stage::off || (v.dlss == Stage::running && !v.dlss_healthy)) return "Degraded: DLSS is off";
    if (v.reflex == Stage::off) return "Degraded: Reflex is off";
    if (v.generation == Generation::unavailable || v.generation == Generation::stopped)
        return "Degraded: frame generation is off";
    return "Active";
}

// The Now lines (v0.28.0): the state in a few words, short enough for the
// value box; the help lines explain.
inline std::string dlss_line(const View &v) {
    if (v.dlss == Stage::starting) return "Starting";
    if (v.dlss == Stage::off) return with_reason("Off", reason_text(v.dlss_reason));
    if (!v.dlss_healthy) return "Stopped (see the log)";
    if (v.dlss == Stage::player_off) return "Off";
    return v.dlss_mode;
}

// The render resolution and the output, or the game's own while DLSS is not
// running.
inline std::string resolution_line(const View &v) {
    if (v.dlss == Stage::starting) return "Starting";
    if (v.dlss != Stage::running || !v.dlss_healthy) return "The game's own";
    if (!v.render_width || !v.render_height || !v.scene_width || !v.scene_height) return "Not known yet";
    if (v.render_width == v.scene_width && v.render_height == v.scene_height) return size_text(v.scene_width, v.scene_height);
    return size_text(v.render_width, v.render_height) + " to " + size_text(v.scene_width, v.scene_height);
}

// A failure is "Off: why"; the player's own Off is plain "Off" (not degraded).
inline std::string reflex_line(const View &v) {
    if (v.reflex == Stage::starting) return "Starting";
    if (v.reflex == Stage::off) return with_reason("Off", reason_text(v.reflex_reason));
    return v.reflex_mode;
}

inline std::string generation_line(const View &v) {
    switch (v.generation) {
    case Generation::off: return "Off";
    case Generation::starting: return "Starting";
    case Generation::unavailable: return with_reason("Not available", reason_text(v.generation_reason));
    case Generation::stopped: return with_reason("Stopped", reason_text(v.generation_reason));
    case Generation::armed: break;
    }
    if (!v.generation_enabled) return "Off";
    // Its inputs come from DLSS.
    if (v.dlss == Stage::player_off) return "Paused (DLSS is off)";
    const bool dynamic = v.generation_mode == 2, automatic = v.generation_mode == 1;
    if (!v.generating) return (dynamic ? std::string("Dynamic") : std::to_string(v.requested) + "x") + ", paused";
    if (dynamic && v.dynamic_applied) {
        // Dynamic never goes past its target: at 1x for a while, the game
        // already runs above half of it (game-81: ~175 fps at 240 Hz).
        if (v.presented_now == 1 && v.single_streak >= auto_pause_presents) return "Dynamic, 1x (target reached)";
        return v.presented_now ? "Dynamic, showing " + std::to_string(v.presented_now) + "x" : std::string("Dynamic, generating");
    }
    const auto shown = std::to_string(v.generated + 1) + "x";
    if (dynamic && v.dynamic_support == 1) return shown + " (no Dynamic here)";
    if (dynamic && v.dynamic_refused) return shown + " (Dynamic refused)";
    if (dynamic) return shown + ", switching to Dynamic";
    if (automatic) return shown + (v.single_streak >= auto_pause_presents ? " Auto, paused" : " Auto, generating");
    if (v.requested > v.generated + 1) return shown + " (the card's most)";
    return shown + ", generating";
}

inline std::string generated_line(const View &v) {
    return v.generation == Generation::off ? "None" : count_text(v.presented_generated);
}

}  // namespace mhw::alpha::status

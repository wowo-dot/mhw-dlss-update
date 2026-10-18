#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "../dlss_sdk/geometry.hpp"
#include "capabilities.hpp"
#include "config.hpp"

// The settings CMFM's "Settings" page shows (v0.28.0), as the ini holds
// them. Every row applies at once; a row that cannot change now stays visible,
// greyed, with the reason in the help line (lock_reason()).
// - "DLSS" On/Off: Off hands the game back (continuous_dlaa::turn_off): its own
//   anti-aliasing, resolution and aspect. "Mode" is the mode it runs at, kept
//   while Off. One ini key holds both: dlss_mode = off or the mode.
// - "Frame generation" On/Off: DLSS-G loads at every start where the card
//   supports it (frame_generation_plugin = auto); it needs DLSS (its inputs
//   come from it), so it is locked while DLSS is Off.
// - "Multiplier": the fixed multipliers this card can show (its DLSS-G limit,
//   read when the page is added).
// - "Pacing": Fixed, Auto pause (NVIDIA's Auto mode pauses a fixed multiplier
//   when the game would run faster without it) or Dynamic (DLSS-G picks the
//   count toward "Target frame rate"; on cards that support it).
// - One ini key holds frame generation, the multiplier and Dynamic:
//   frame_generation = off, 2x...6x or dynamic; frame_generation_auto holds
//   Auto pause. While Off or Dynamic the multiplier is kept in memory.
// - "Reflex": Off only while DLSS-G is not loaded (it paces the swap chain and
//   needs Reflex); turning frame generation on turns Reflex on again.
// Pure (tests/settings_menu_tests.cpp).
namespace mhw::settings::menu_model {

struct Settings {
    dlss_sdk::ReconstructionMode dlss_mode = dlss_sdk::ReconstructionMode::performance;
    bool dlss = true; // false: the player's Off (dlss_mode is the mode it comes back with)
    bool frame_generation = true;
    unsigned multiplier = 2;
    ReflexMode reflex = ReflexMode::on;
    bool dynamic{};
    unsigned dynamic_target{}; // 0: the display's refresh rate
    bool generation_auto{};
};

// "Mode" choices, highest quality first.
inline constexpr std::array<dlss_sdk::ReconstructionMode, 4> modes{
    dlss_sdk::ReconstructionMode::dlaa, dlss_sdk::ReconstructionMode::quality, dlss_sdk::ReconstructionMode::balanced,
    dlss_sdk::ReconstructionMode::performance};
inline constexpr const char *mode_names[]{"DLAA", "Quality", "Balanced", "Performance"};
static_assert(std::size(mode_names) == modes.size());
// The CHOICE index of a mode (that of Performance for any other value).
constexpr int choice_of(dlss_sdk::ReconstructionMode mode) {
    for (unsigned i = 0; i < modes.size(); ++i)
        if (modes[i] == mode) return static_cast<int>(i);
    return static_cast<int>(modes.size()) - 1;
}
// The mode at a CHOICE index; false outside the list.
constexpr bool mode_at(int index, dlss_sdk::ReconstructionMode &mode) {
    if (index < 0 || index >= static_cast<int>(modes.size())) return false;
    mode = modes[static_cast<unsigned>(index)];
    return true;
}

// "Reflex" choices, the default first.
inline constexpr std::array<ReflexMode, 3> reflex_modes{ReflexMode::on, ReflexMode::boost, ReflexMode::off};
inline constexpr const char *reflex_names[]{"On", "Boost", "Off"};
static_assert(std::size(reflex_names) == reflex_modes.size());
constexpr int choice_of(ReflexMode mode) {
    for (unsigned i = 0; i < reflex_modes.size(); ++i)
        if (reflex_modes[i] == mode) return static_cast<int>(i);
    return 0;
}
// The mode at a CHOICE index; false outside the list.
constexpr bool reflex_at(int index, ReflexMode &mode) {
    if (index < 0 || index >= static_cast<int>(reflex_modes.size())) return false;
    mode = reflex_modes[static_cast<unsigned>(index)];
    return true;
}
// Whether `mode` can be chosen while the frame generation setting is
// `generation` and DLSS-G is `loaded`; if not, the reason for the help line.
inline bool reflex_allowed(ReflexMode mode, bool generation, bool loaded, std::string &why) {
    if (mode != ReflexMode::off || (!generation && !loaded)) return true;
    why = generation
              ? "Frame generation needs Reflex; turn frame generation off first."
              : "Reflex stays on while DLSS Frame Generation is loaded (frame_generation_plugin in mhw_dlss.ini).";
    return false;
}
// The sl::ReflexMode value (reflex_mode.hpp): 0 off, 1 low latency, 2 boost.
constexpr unsigned reflex_sdk_value(ReflexMode mode) {
    return mode == ReflexMode::off ? 0U : mode == ReflexMode::boost ? 2U : 1U;
}

// The fixed multipliers (total frames per rendered frame) the mod offers.
inline constexpr std::array<unsigned, 4> multipliers{2, 3, 4, 6};
// The "Multiplier" choices for a card that shows `max` frames at most (0: not
// known, every one).
inline std::vector<unsigned> multiplier_choices(unsigned max) {
    std::vector<unsigned> out;
    for (const auto m : multipliers)
        if (!max || m <= max) out.push_back(m);
    if (out.empty()) out.push_back(multipliers.front());
    return out;
}
inline std::string multiplier_name(unsigned m) {
    return std::to_string(m) + "x";
}
// The CHOICE index of `m`: its own, else the largest choice below it, else 0.
inline int multiplier_index(const std::vector<unsigned> &choices, unsigned m) {
    int best = 0;
    for (unsigned i = 0; i < choices.size(); ++i)
        if (choices[i] <= m) best = static_cast<int>(i);
    return best;
}

// "Pacing" choices; Dynamic only where the card supports it (or it is not known).
enum class Pacing : unsigned { fixed, auto_pause, dynamic };
inline constexpr const char *pacing_names[]{"Fixed", "Auto pause", "Dynamic"};
inline constexpr unsigned pacing_count(bool dynamic_offered) {
    return dynamic_offered ? 3U : 2U;
}
constexpr Pacing pacing(const Settings &s, bool dynamic_offered) {
    return s.dynamic && dynamic_offered ? Pacing::dynamic : s.generation_auto ? Pacing::auto_pause : Pacing::fixed;
}
// Applies a pacing: Dynamic keeps the Auto pause setting for the way back.
constexpr void set_pacing(Settings &s, Pacing p) {
    s.dynamic = p == Pacing::dynamic;
    if (p != Pacing::dynamic) s.generation_auto = p == Pacing::auto_pause;
}
// The session's mode (frame_generation::game::Mode order: fixed, automatic, dynamic).
constexpr unsigned generation_mode(const Settings &s) {
    return s.dynamic ? 2U : s.generation_auto ? 1U : 0U;
}

// "Target frame rate" choices: the display's refresh rate (0), then common
// rates; a value from the ini that is not one of them is added in order.
inline constexpr std::array<unsigned, 18> targets{0,   60,  72,  75,  90,  100, 120, 144, 165,
                                                  180, 200, 240, 280, 300, 360, 480, 500, 540};
inline std::vector<unsigned> target_choices(unsigned configured) {
    std::vector<unsigned> out(targets.begin(), targets.end());
    if (std::find(out.begin(), out.end(), configured) == out.end()) {
        out.push_back(configured);
        std::sort(out.begin(), out.end());
    }
    return out;
}
inline std::string target_name(unsigned target) {
    return target ? std::to_string(target) + " fps" : "Display refresh";
}
// The CHOICE index of `target` in `choices`; -1 if it is not one of them.
inline int target_index(const std::vector<unsigned> &choices, unsigned target) {
    const auto at = std::find(choices.begin(), choices.end(), target);
    return at == choices.end() ? -1 : static_cast<int>(at - choices.begin());
}

// What the Settings page offers, fixed when it is added (CMFM keeps its
// choices): the card's multipliers and whether Dynamic is one of the pacings.
struct Offer {
    std::vector<unsigned> multipliers = multiplier_choices(0);
    bool dynamic = true;
};
inline Offer offer_for(const capabilities::Capabilities &c) {
    Offer o;
    o.multipliers = multiplier_choices(c.frame_generation == capabilities::Support::no ? 0 : c.max_multiplier);
    o.dynamic = c.dynamic != capabilities::Support::no;
    return o;
}

// The Settings page's rows.
enum class Row : unsigned { dlss, mode, frame_generation, multiplier, pacing, target, reflex };
// Why `row` cannot change now (the help line while it is greyed), or empty.
// `fg_requested`: DLSS-G loads at start (frame_generation_plugin = auto).
inline std::string lock_reason(Row row, const Settings &s, const capabilities::Capabilities &c, const Offer &offer,
                               bool fg_requested) {
    namespace caps = capabilities;
    std::string why;
    switch (row) {
    case Row::dlss: return caps::available(c, caps::Option::dlss, 0, why) ? std::string() : why;
    case Row::mode:
        if (!caps::available(c, caps::Option::dlss, 0, why)) return why;
        return s.dlss ? std::string() : "Turn DLSS on first.";
    case Row::reflex: return caps::available(c, caps::Option::reflex, 0, why) ? std::string() : why;
    case Row::frame_generation:
    case Row::multiplier:
    case Row::pacing:
    case Row::target: break;
    }
    if (!fg_requested) return caps::frame_generation_reason({.missing = caps::Missing::plugin_off});
    if (!caps::available(c, caps::Option::frame_generation, 0, why)) return why;
    // Its inputs (depth, motion, the HUD-less image) come from DLSS.
    if (!s.dlss) return "Frame generation needs DLSS; turn DLSS on first.";
    if (row == Row::frame_generation) return {};
    if (!s.frame_generation) return "Turn frame generation on first.";
    const bool dynamic = pacing(s, offer.dynamic) == Pacing::dynamic;
    if (row == Row::multiplier && dynamic) return "Dynamic pacing picks the number of frames itself.";
    if (row == Row::multiplier && offer.multipliers.size() == 1)
        return "This graphics card shows " + multiplier_name(offer.multipliers.front()) +
               " at most; 3x and up need an RTX 50.";
    if (row == Row::target && !dynamic) return "Used only by Dynamic pacing.";
    return {};
}

} // namespace mhw::settings::menu_model

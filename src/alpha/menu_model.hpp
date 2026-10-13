#pragma once
#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "../dlss_sdk/geometry.hpp"
#include "config.hpp"

// The settings CMFM's "Settings" page shows, as the ini holds them (alpha).
// - "DLSS mode" changes at once while DLSS runs: the worker rebuilds the
//   pipeline at the new render scale (continuous_dlaa::change_mode). Off
//   hands the game back (continuous_dlaa::turn_off): its own anti-aliasing,
//   resolution and aspect; the last mode is kept in memory for the way back.
// - "Frame generation" Off/On changes at once: DLSS-G loads at every start
//   where the card supports it (frame_generation_plugin = auto); with
//   frame_generation_plugin = off it never loads and the row is locked.
// - "Multiplier" changes at once while frame generation runs: 2x to 6x, or
//   Dynamic (DLSS-G picks the count toward "Target frame rate").
// - One ini key holds both: Off writes "off", On writes the multiplier or
//   "dynamic". While frame generation is Off the choice is kept in memory, so
//   On restores it; while Dynamic, the last fixed multiplier is kept too.
// - "Target frame rate" (dynamic_target) and "Auto pause"
//   (frame_generation_auto) change at once; the first is used only by Dynamic,
//   the second only by fixed multipliers.
// - "Reflex" changes at once. Off only while DLSS-G is not loaded (it paces
//   the swap chain and needs Reflex); turning frame generation on turns
//   Reflex on again.
// Pure (tests/alpha_menu_tests.cpp).
namespace mhw::alpha::menu_model {

struct Settings {
    dlss_sdk::ReconstructionMode dlss_mode = dlss_sdk::ReconstructionMode::performance;
    bool dlss = true;  // false: the player's Off (dlss_mode is the mode it comes back with)
    bool frame_generation = true;
    unsigned multiplier = 2;
    ReflexMode reflex = ReflexMode::on;
    bool dynamic{};
    unsigned dynamic_target{};  // 0: the display's refresh rate
    bool generation_auto{};
};

// "DLSS mode" choices: Off, then the modes, highest quality first.
inline constexpr std::array<dlss_sdk::ReconstructionMode, 4> modes{
    dlss_sdk::ReconstructionMode::dlaa, dlss_sdk::ReconstructionMode::quality, dlss_sdk::ReconstructionMode::balanced,
    dlss_sdk::ReconstructionMode::performance};
inline constexpr const char *mode_names[]{"Off", "DLAA", "Quality", "Balanced", "Performance"};
inline constexpr int off_choice = 0;
static_assert(std::size(mode_names) == modes.size() + 1);
// The CHOICE index of a mode (that of Performance for any other value).
constexpr int choice_of(dlss_sdk::ReconstructionMode mode) {
    for (unsigned i = 0; i < modes.size(); ++i)
        if (modes[i] == mode) return static_cast<int>(i) + 1;
    return static_cast<int>(modes.size());
}
// The CHOICE index of the DLSS setting: Off or the mode.
constexpr int dlss_choice(const Settings &s) { return s.dlss ? choice_of(s.dlss_mode) : off_choice; }
// The setting at a CHOICE index: Off (`mode` unchanged) or a mode; false
// outside the list.
constexpr bool mode_at(int index, bool &dlss, dlss_sdk::ReconstructionMode &mode) {
    if (index < 0 || index > static_cast<int>(modes.size())) return false;
    dlss = index != off_choice;
    if (dlss) mode = modes[static_cast<unsigned>(index - 1)];
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
    why = generation ? "Frame generation needs Reflex; turn frame generation off first."
                     : "Reflex stays on while DLSS Frame Generation is loaded (frame_generation_plugin in mhw_dlss.ini).";
    return false;
}
// The sl::ReflexMode value (reflex_mode.hpp): 0 off, 1 low latency, 2 boost.
constexpr unsigned reflex_sdk_value(ReflexMode mode) {
    return mode == ReflexMode::off ? 0U : mode == ReflexMode::boost ? 2U : 1U;
}

// "Multiplier" choices: the fixed multipliers, then Dynamic.
inline constexpr std::array<unsigned, 4> multipliers{2, 3, 4, 6};
inline constexpr const char *multiplier_names[]{"2x", "3x", "4x", "6x", "Dynamic"};
inline constexpr int dynamic_choice = static_cast<int>(multipliers.size());
static_assert(std::size(multiplier_names) == multipliers.size() + 1);

// The CHOICE index of a multiplier (that of 2x for any other value).
constexpr int choice_of(unsigned multiplier) {
    for (unsigned i = 0; i < multipliers.size(); ++i)
        if (multipliers[i] == multiplier) return static_cast<int>(i);
    return 0;
}
// The CHOICE index of the frame generation setting: Dynamic or the multiplier.
constexpr int generation_choice(const Settings &s) { return s.dynamic ? dynamic_choice : choice_of(s.multiplier); }
// The multiplier at a CHOICE index; 0 outside the list and for Dynamic.
constexpr unsigned multiplier_at(int index) {
    return index >= 0 && index < static_cast<int>(multipliers.size()) ? multipliers[static_cast<unsigned>(index)] : 0;
}

// "Target frame rate" choices: the display's refresh rate (0), then common
// rates; a value from the ini that is not one of them is added in order.
inline constexpr std::array<unsigned, 18> targets{0, 60, 72, 75, 90, 100, 120, 144, 165, 180, 200, 240, 280, 300, 360, 480, 500, 540};
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

// The session's mode (frame_generation::game::Mode order: fixed, automatic, dynamic).
constexpr unsigned generation_mode(const Settings &s) { return s.dynamic ? 2U : s.generation_auto ? 1U : 0U; }


}  // namespace mhw::alpha::menu_model

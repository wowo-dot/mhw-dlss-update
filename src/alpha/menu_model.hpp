#pragma once
#include <array>
#include <string>
#include "../dlss_sdk/geometry.hpp"

// The settings CMFM's "Settings" page shows, as the ini holds them (alpha).
// - "DLSS mode" needs a restart: the pipeline is sized at start.
// - "Frame generation" Off/On needs a restart: DLSS-G loads only at start.
// - "Multiplier" changes at once while frame generation runs.
// - One ini key holds both: Off writes "off", On writes the multiplier. While
//   frame generation is Off the multiplier is kept in memory, so On restores it.
// Pure (tests/alpha_menu_tests.cpp).
namespace mhw::alpha::menu_model {

struct Settings {
    dlss_sdk::ReconstructionMode dlss_mode = dlss_sdk::ReconstructionMode::performance;
    bool frame_generation = true;
    unsigned multiplier = 2;
};

// "DLSS mode" choices, highest quality first.
inline constexpr std::array<dlss_sdk::ReconstructionMode, 4> modes{
    dlss_sdk::ReconstructionMode::dlaa, dlss_sdk::ReconstructionMode::quality, dlss_sdk::ReconstructionMode::balanced,
    dlss_sdk::ReconstructionMode::performance};
inline constexpr const char *mode_names[]{"DLAA", "Quality", "Balanced", "Performance"};
static_assert(std::size(mode_names) == modes.size());
constexpr int choice_of(dlss_sdk::ReconstructionMode mode) {
    for (unsigned i = 0; i < modes.size(); ++i)
        if (modes[i] == mode) return static_cast<int>(i);
    return 3;
}
// The mode at a CHOICE index; false outside the list.
constexpr bool mode_at(int index, dlss_sdk::ReconstructionMode &mode) {
    if (index < 0 || index >= static_cast<int>(modes.size())) return false;
    mode = modes[static_cast<unsigned>(index)];
    return true;
}

inline constexpr std::array<unsigned, 4> multipliers{2, 3, 4, 6};
inline constexpr const char *multiplier_names[]{"2x", "3x", "4x", "6x"};
static_assert(std::size(multiplier_names) == multipliers.size());

// The CHOICE index of a multiplier (that of 2x for any other value).
constexpr int choice_of(unsigned multiplier) {
    for (unsigned i = 0; i < multipliers.size(); ++i)
        if (multipliers[i] == multiplier) return static_cast<int>(i);
    return 0;
}
// The multiplier at a CHOICE index; 0 outside the list.
constexpr unsigned multiplier_at(int index) {
    return index >= 0 && index < static_cast<int>(multipliers.size()) ? multipliers[static_cast<unsigned>(index)] : 0;
}

// Whether the graphics card can show `multiplier`. `maximum` is the number of
// generated frames DLSS-G allows (0 while unknown: everything is accepted and
// the session limits it). If not, the reason for the help line.
inline bool multiplier_allowed(unsigned multiplier, unsigned maximum, std::string &why) {
    if (!maximum || multiplier <= maximum + 1) return true;
    why = maximum == 1 ? "This graphics card shows 2x at most; 3x and up need an RTX 50."
                       : "This graphics card shows " + std::to_string(maximum + 1) + "x at most.";
    return false;
}

}  // namespace mhw::alpha::menu_model

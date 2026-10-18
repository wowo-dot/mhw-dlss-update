#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>

namespace mhw::continuous_dlaa::game_options {
// The game's own DLSS rules, followed for this mod's DLSS
// (docs/research/game-aa-option.md; clean image a78ea69b...):
// - while DLSS runs, the game's anti-aliasing applies as Off and its
//   resolution scaling as High (the game's own DLSS sets TAA and High);
// - the PC graphics menu (uGUIGraphicsOption) shows both rows locked with the
//   game's own help texts, and its "restore defaults" leaves them;
// - the player's saved choices are never written. The player's DLSS Off, a
//   fault or a stop applies them again.
// v0.30.1, FidelityFX CAS (docs/research/game-cas-option.md): the game makes
// CAS and its DLSS exclusive (its display menu turns CAS off when DLSS goes
// on, 0x1e89ea0; both saved on turns both off at start, 0x43e220). CAS
// renders the scene at 0.8 and sharpens it back up (0x43de50), which this
// mod's render scale replaces, so:
// - while DLSS runs, CAS applies as Off, with the player's sharpness kept;
// - the display menu's CAS and CAS sharpness rows are locked (greyed by the
//   game's own row check, 0xeeae80, as it greys the sharpness with CAS off).

// The graphics menu's item ids of the two rows, the game's help texts for
// them while NVIDIA DLSS is on (steam_text 552 and 553; menu ids are
// 10000 + the text's index), and the values applied meanwhile.
constexpr unsigned char resolution_scaling_item = 15, anti_aliasing_item = 22;
constexpr int resolution_scaling_help = 10552, anti_aliasing_help = 10553;
constexpr unsigned char anti_aliasing_off = 0, resolution_scaling_high = 2;
// The game's value ranges: anti-aliasing Off/TAA/FXAA/TAA+FXAA, resolution
// scaling Low/Mid/High/Variable (resolution)/Variable (frame rate).
constexpr unsigned char anti_aliasing_values = 4, resolution_scaling_values = 5;
constexpr bool locked_row(unsigned item) noexcept {
    return item == resolution_scaling_item || item == anti_aliasing_item;
}
constexpr int locked_help(unsigned item, int help) noexcept {
    return item == resolution_scaling_item ? resolution_scaling_help
           : item == anti_aliasing_item    ? anti_aliasing_help
                                           : help;
}
// The display menu's rows (0xeeae80's descriptors: kind at +0, index at
// +20): kind 2 are the display options, of which 13 is FidelityFX CAS and 14
// its sharpness (0x1e8cb00 and 0x1e8ab40 apply them).
constexpr unsigned display_option_kind = 2;
constexpr unsigned char cas_row = 13, cas_sharpness_row = 14;
constexpr bool locked_display_row(unsigned kind, unsigned index) noexcept {
    return kind == display_option_kind && (index == cas_row || index == cas_sharpness_row);
}

// Worker, at the DLSS install: checks the game's code, reads the player's
// saved choices and hooks the game's apply functions and three menu
// functions, enabled at once and passing everything through until
// take_over(). False (nothing hooked) when the code differs.
bool install(std::uintptr_t image, std::string &reason);
// Worker: this mod's DLSS runs from now (its install, or leaving Off). The
// game's anti-aliasing Off, CAS Off and resolution scaling High, through the
// game's own setter and apply functions. Before render_resolution takes the
// scale; CAS's content scale reaches the render thread on its next update.
void take_over() noexcept;
// Worker: the player's DLSS Off, a fault or a stop. The player's choices
// again, after render_resolution released the scale (released()).
void give_back() noexcept;
bool taken() noexcept;
void write(std::ostream &);
} // namespace mhw::continuous_dlaa::game_options

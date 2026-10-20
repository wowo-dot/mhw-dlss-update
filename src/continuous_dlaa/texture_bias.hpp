#pragma once
#include "texture_bias_policy.hpp"
#include <cstdint>
#include <iosfwd>

namespace mhw::continuous_dlaa::texture_bias {
// The texture mip bias (docs/research/texture-mip-bias.md; clean image
// a78ea69b...). While DLSS draws below the output size, materials sample one
// mip sharper per halving, as NVIDIA's DLSS guide asks (texture_bias_policy.hpp),
// from the game's own sampler banks:
// - materials bake bank[sScene mSamplerLodBias] when they load. At install
//   that becomes material_bank, whose descriptors are bank 0's copied over;
// - every bound sampler state goes through 0x263daf0 (its anisotropy variant).
//   The hook hands a material bank state's pick to the same state of the bank
//   the mode wants; everything else passes through;
// - DLSS Off, a stop or a fault is bank 0: the game's own textures;
// - the resident sampler table cache never evicts: a guard keeps it from
//   filling (room_for_new_set).

// The image for the read-only measurement in dlss_summary.
void observe(std::uintptr_t image) noexcept;
enum class Install : unsigned { installed, retry, refused };
// texture_bias = on: the bank, the copy and the hook. retry while the game
// hasn't built its banks yet; refused for good (code gates, banks not as
// tested, another mod's bank).
Install install(std::uintptr_t image) noexcept;
bool installed() noexcept;
// Why not (worker), empty once installed.
const char *refusal() noexcept;
// Worker, each loop: the render scale DLSS draws at (render / output width),
// 0 while it doesn't draw. A change of bank is logged to `log`.
void follow(float scale, std::ostream &log) noexcept;
// dlss_summary's "texture_bias" object.
void write(std::ostream &out);
} // namespace mhw::continuous_dlaa::texture_bias

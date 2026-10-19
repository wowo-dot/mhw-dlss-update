#pragma once
#include <cstdint>
#include <iosfwd>

namespace mhw::continuous_dlaa::texture_bias {
// The texture mip bias (docs/research/texture-mip-bias.md; clean image
// a78ea69b...). The game builds 15 sampler banks once; a material bakes
// bank[sScene mSamplerLodBias] when it loads; each bound sampler's handles
// go through the resident sampler table cache, which never evicts (2048
// slots, no bound check). 0.31.0-dev.1 only measures, read-only: how full
// that cache gets, the bank materials take, and each bank's bias.

// The image base for the reads; until then write() reports nothing.
void install(std::uintptr_t image) noexcept;
// dlss_summary's "texture_bias" object.
void write(std::ostream &out);
} // namespace mhw::continuous_dlaa::texture_bias

// scaled_temporal::silhouette_material (v0.28.6): which hidden-surface material
// values the silhouette correction accepts.
#include "../src/scaled_temporal/silhouette_material.hpp"
#include <bit>
#include <cstdio>
#include <limits>

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}
std::array<unsigned, 5> material(unsigned lighting, unsigned mask, float intensity) {
    return {0, 0, lighting, mask, std::bit_cast<unsigned>(intensity)};
}
}  // namespace

int main() {
    using mhw::scaled_temporal::silhouette_material;
    check(silhouette_material(material(1, 7680, .1F)), "0.1 (our captures)");
    check(silhouette_material(material(1, 7680, .02F)), "0.02 (a player's game, the Fatalis fight)");
    check(silhouette_material(material(1, 7680, 0.F)), "0 (an invisible silhouette)");
    check(silhouette_material(material(1, 7680, 1.F)), "1");
    check(!silhouette_material(material(0, 7680, .1F)), "custom lighting off");
    check(!silhouette_material(material(1, 0, .1F)), "another G-buffer mask");
    check(!silhouette_material(material(1, 7680, -.1F)), "negative intensity");
    check(!silhouette_material(material(1, 7680, std::numeric_limits<float>::quiet_NaN())), "NaN intensity");
    check(!silhouette_material(material(1, 7680, std::numeric_limits<float>::infinity())), "infinite intensity");
    if (failures) return 1;
    std::puts("silhouette_material_tests: ok");
    return 0;
}

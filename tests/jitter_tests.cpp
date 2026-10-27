// The camera jitter while DLSS draws (src/dlss_sdk/jitter.hpp).
#include "../src/dlss_sdk/jitter.hpp"
#include <cmath>
#include <iostream>
using namespace mhw::dlss_sdk;

static_assert(halton(1, 2) == .5F && halton(2, 2) == .25F && halton(3, 2) == .75F && halton(4, 2) == .125F);
static_assert(halton(0, 2) == 0.F && halton(0, 3) == 0.F);

int main() {
    int failures{};
    const auto check = [&](bool ok, const char *what) {
        if (!ok) {
            std::cerr << "failed: " << what << '\n';
            ++failures;
        }
    };
    const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-6F; };
    check(near(halton(1, 3), 1.F / 3) && near(halton(2, 3), 2.F / 3) && near(halton(3, 3), 1.F / 9) &&
              near(halton(4, 3), 4.F / 9),
          "Halton base 3");
    // The game's own table (0x227ff00): indices 20 to 39 minus 0.5; 20 is 10100b.
    check(near(halton(20, 2) - .5F, -.34375F), "the game's first table entry");

    const auto geometry = [](unsigned rw, unsigned rh, ReconstructionMode m) {
        return Geometry{{rw, rh}, {rw, rh}, {2560, 1440}, m};
    };
    check(jitter_phases(Geometry::dlaa(2560, 1440)) == 8, "DLAA: 8 phases");
    check(jitter_phases(geometry(1696, 954, ReconstructionMode::quality)) == 19, "Quality at 2560x1440: 19");
    check(jitter_phases(geometry(1472, 828, ReconstructionMode::balanced)) == 25, "Balanced at 2560x1440: 25");
    check(jitter_phases(geometry(1280, 720, ReconstructionMode::performance)) == 32, "Performance: 32");
    check(jitter_phases(Geometry{}) == 0 && jitter_offset(7, 0).x == 0.F, "no geometry: no phases, no offset");

    // Each period covers the whole pixel, evenly, and repeats.
    for (const unsigned phases : {8U, 19U, 25U, 32U}) {
        float x_min = 1, x_max = -1, y_min = 1, y_max = -1, x_sum = 0, y_sum = 0;
        bool inside = true;
        for (unsigned f = 0; f < phases; ++f) {
            const auto o = jitter_offset(1000 + f, phases);
            inside = inside && o.x >= -.5F && o.x < .5F && o.y >= -.5F && o.y < .5F;
            x_min = std::fmin(x_min, o.x), x_max = std::fmax(x_max, o.x);
            y_min = std::fmin(y_min, o.y), y_max = std::fmax(y_max, o.y);
            x_sum += o.x, y_sum += o.y;
        }
        check(inside, "offsets within half a pixel");
        check(x_min < -.35F && x_max > .35F && y_min < -.35F && y_max > .35F, "the whole pixel on both axes");
        check(std::fabs(x_sum / phases) < .1F && std::fabs(y_sum / phases) < .1F, "centred");
        check(jitter_offset(1000, phases).x == jitter_offset(1000 + phases, phases).x &&
                  jitter_offset(1000, phases).y == jitter_offset(1000 + phases, phases).y,
              "a period of `phases` frames");
    }

    // Written in the game's units, the projection maps back to the offset
    // (projection_jitter, +Y down for DLSS); the game's table reaches only a
    // quarter of a pixel.
    const auto g = geometry(1696, 954, ReconstructionMode::quality);
    for (unsigned f = 0; f < 19; ++f) {
        const auto o = jitter_offset(f, 19);
        PixelJitter measured{};
        check(projection_jitter(g, camera_jitter(o), 1696, 954, measured) && near(measured.x, o.x) &&
                  near(measured.y, -o.y),
              "the measured offset is the one written");
    }
    float table_reach = 0;
    for (unsigned i = 20; i < 40; ++i) {
        PixelJitter measured{};
        projection_jitter(g, {halton(i, 2) - .5F, halton(i, 3) - .5F}, 1696, 954, measured);
        table_reach = std::fmax(table_reach, std::fmax(std::fabs(measured.x), std::fabs(measured.y)));
    }
    check(table_reach <= .25F, "the game's table covers only the middle half");
    if (failures) return 1;
    std::cout << "jitter tests passed\n";
    return 0;
}

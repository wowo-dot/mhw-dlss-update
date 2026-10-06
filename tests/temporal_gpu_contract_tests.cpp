#include "../src/temporal_gpu_inputs/contract.hpp"
#include <bit>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace ti = mhw::temporal_gpu_inputs;
void check(bool v) { if (!v) throw std::runtime_error("GPU input contract test failed"); }
int main() {
    try {
        std::vector<std::uint32_t> color(1, 0), depth(1, std::bit_cast<std::uint32_t>(.5f)), motion(1, 0);
        ti::Frame f{1, 1, color, depth, motion}; std::string reason;
        check(ti::validate(f, reason) && reason.empty());
        auto bad = f; bad.width = 0; check(!ti::validate(bad, reason));
        bad = f; bad.height = 8193; check(!ti::validate(bad, reason));
        bad = f; bad.width = 4096; bad.height = 4096; check(!ti::validate(bad, reason));
        bad = f; bad.motion = {}; check(!ti::validate(bad, reason));
        for (const auto d : {0x7f800000U, 0x7fc00000U, 0xbf800000U, 0x40000000U}) {
            depth[0] = d; check(!ti::validate(f, reason));
        }
        depth[0] = std::bit_cast<std::uint32_t>(1.0f); check(ti::validate(f, reason));
        for (const auto c : {31U << 6, 31U << 17, 31U << 27}) {
            color[0] = c; check(!ti::validate(f, reason));
        }
        color[0] = 0;
        for (const auto m : {0x7c000000U, 0x00007c00U, 0xfe000000U, 0x0000fe00U}) {
            motion[0] = m; check(!ti::validate(f, reason));
        }
        const auto one = (15U << 6) | (15U << 17) | (15U << 27);
        check(ti::color_reference(one) == std::array<float, 4>{1, 1, 1, 1});
        check(ti::motion_reference(0x3c00bc01, 1920, 1080) == std::array<float, 2>{-960, -540});
        check(ti::motion_reference(0xbc003c00, 1920, 1080) == std::array<float, 2>{960, 540});
        check(ti::motion_reference(1, 2, 2) == std::array<float, 2>{0, 0});
        check(ti::motion_reference(0x00010000, 2, 2)[0] == -std::ldexp(1.0f, -24));
        std::cout << "Extent, plane size, finite/depth range and independent packed-value controls passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

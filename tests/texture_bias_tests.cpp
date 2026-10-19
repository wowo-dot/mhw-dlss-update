// The texture mip bias's rules (src/continuous_dlaa/texture_bias_policy.hpp).
#include "../src/continuous_dlaa/texture_bias_policy.hpp"
#include <cmath>
#include <iostream>
using namespace mhw::continuous_dlaa::texture_bias;

int main() {
    int failures{};
    const auto check = [&](bool ok, const char *what) {
        if (!ok) {
            std::cerr << "failed: " << what << '\n';
            ++failures;
        }
    };
    const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4F; };
    // The banks as run 54460 read them from the game.
    check(bank_bias(0) == 0.F && near(bank_bias(1), -1.F) && near(bank_bias(2), -1.32193F) &&
              near(bank_bias(3), -1.58496F) && near(bank_bias(4), -1.80735F) && near(bank_bias(5), -2.F) &&
              near(bank_bias(6), -2.16992F) && near(bank_bias(7), -2.32193F),
          "banks 0-7");
    check(near(bank_bias(8), 1.F) && near(bank_bias(14), 2.32193F) && bank_bias(15) == 0.F, "banks 8-14, none after");
    check(bank_bias(material_bank) > 0.F, "the material bank is one the shipped game never binds");
    // NVIDIA's log2(scale) - 1 at each mode, as the render scales the mod uses.
    check(bank_for(1.F) == 1, "DLAA: -1");
    check(bank_for(1696.F / 2560.F) == 3 && bank_for(2.F / 3.F) == 3, "Quality: -1.58");
    check(bank_for(1484.F / 2560.F) == 4 && bank_for(0.58F) == 4, "Balanced: -1.81 for -1.79");
    check(bank_for(0.5F) == 5 && bank_for(1280.F / 2560.F) == 5, "Performance: -2");
    check(bank_for(0.F) == 0 && bank_for(-1.F) == 0 && bank_for(NAN) == 0 && bank_for(1.5F) == 0,
          "no scale: the game's own");
    check(bank_for(0.3F) == 7, "below the ladder: its last bank");
    // The guard. A fresh session: the hub at about 433 slots, bank 0 at
    // level 0 (samplers without variants) and the player's level 4.
    Room fresh{433, 2048, min_set_cost, level_cost, room_margin, 0b10001};
    check(room_for_new_set(fresh), "a first bias bank fits");
    fresh.slots = 433 + 4 * 60;
    check(room_for_new_set(fresh), "all four modes' banks fit (run 26960: 40 to 70 slots each)");
    fresh.slots = 1600;
    check(!room_for_new_set(fresh), "near the reserve for the three levels bank 0 hasn't seen: none");
    // After the player stepped through every anisotropy level (run 54460: 1329).
    Room stepped{1329, 2048, min_set_cost, level_cost, room_margin, 0b11111};
    check(room_for_new_set(stepped), "bank 0 has every level: room while the bank's set fits");
    stepped.slots = 1856;
    check(room_for_new_set(stepped), "1856 + 64 + 128 fits");
    stepped.slots = 1857;
    check(!room_for_new_set(stepped), "1857 + 64 + 128 doesn't");
    stepped.set_cost = 225;
    stepped.slots = 1700;
    check(!room_for_new_set(stepped), "a larger measured set needs more room");
    check(!room_for_new_set({0, 2048, 0xffffffffU, 0xffffffffU, 0, 0}), "no overflow in the sum");
    if (failures) return 1;
    std::cout << "texture bias tests passed\n";
    return 0;
}

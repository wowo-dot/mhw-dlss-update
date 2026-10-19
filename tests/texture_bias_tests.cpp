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
    // The guard. A fresh session (hub, ~433 slots, bank 0 at one level).
    Room fresh{433, 2048, 225, room_margin, 0b1};
    check(room_for_new_set(fresh), "a first bias set fits");
    fresh.slots = 658;
    check(room_for_new_set(fresh), "a second one");
    fresh.slots = 883;
    check(!room_for_new_set(fresh), "a third leaves too little for the game's own sets at the other four levels");
    // After the player stepped through every anisotropy level (run 54460: 1329).
    Room stepped{1329, 2048, 225, room_margin, 0b11111};
    check(room_for_new_set(stepped), "bank 0 has all its sets: room for more");
    stepped.slots = 1554;
    check(room_for_new_set(stepped), "1554 + 225 + 128 fits");
    stepped.slots = 1779;
    check(!room_for_new_set(stepped), "1779 + 225 + 128 doesn't");
    check(!room_for_new_set({0, 2048, 0xffffffffU, 0, 0}), "no overflow in the sum");
    if (failures) return 1;
    std::cout << "texture bias tests passed\n";
    return 0;
}

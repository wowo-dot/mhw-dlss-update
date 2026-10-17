#include "../src/continuous_dlaa/native_taa.hpp"
#include "../src/continuous_dlaa/resolve.hpp"
#include "../src/continuous_dlaa/shader_rejection.hpp"
#include <memory>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace cd = mhw::continuous_dlaa;
namespace tb = mhw::temporal_bindings;
void check(bool value, const char *reason) {
    if (!value) {
        std::cerr << reason << '\n';
        std::exit(1);
    }
}
int main() {
    tb::Snapshot s;
    s.technique = 23086;
    tb::ShaderRecipe p;
    p.technique = 23086;
    p.size = cd::native_taa_shader_bytes;
    p.owner_count = p.binding_count = 7;
    constexpr unsigned roles[]{0, 1, 2, 3, 5, 6, 4};
    for (unsigned i = 0; i < 7; ++i) {
        const auto role = roles[i];
        s.inputs[role].object = 100 + role;
        s.inputs[role].resource = 1000 + role;
        s.inputs[role].id = 10 + role;
        p.owners[i] = {10 + role, 0, 100 + role};
        p.bindings[i] = {i == 6 ? 3U | (i << 4) : 2U | (i << 4) | (i << 24), 200 + i, 300 + i};
    }
    check(cd::single_native_output(s, p), "exact six-SRV/one-UAV recipe");
    for (unsigned i = 0; i < 7; ++i) {
        auto changed = p;
        changed.bindings[i].cpu_descriptor = 0;
        check(!cd::single_native_output(s, changed), "missing descriptor rejected");
        changed = p;
        changed.owners[i].object += 1;
        check(!cd::single_native_output(s, changed), "owner substitution rejected");
        changed = p;
        changed.owners[i].id += 1;
        check(!cd::single_native_output(s, changed), "binding identity substitution rejected");
    }
    auto altered = p;
    altered.binding_count = 6;
    check(!cd::single_native_output(s, altered), "missing UAV rejected");
    altered = p;
    altered.bindings[7] = p.bindings[6];
    altered.binding_count = 8;
    check(!cd::single_native_output(s, altered), "extra UAV rejected");
    altered = p;
    altered.bindings[6].word |= 1U << 24;
    check(!cd::single_native_output(s, altered), "wrong UAV slot rejected");
    altered = p;
    altered.bindings[0].word = 15;
    check(!cd::single_native_output(s, altered), "unknown binding kind rejected");
    altered = p;
    altered.bindings[1] = altered.bindings[0];
    check(!cd::single_native_output(s, altered), "duplicate SRV rejected");
    altered = p;
    altered.bindings[0].word = 2U | (7U << 4);
    check(!cd::single_native_output(s, altered), "out-of-range owner rejected");
    altered = p;
    altered.size++;
    check(!cd::single_native_output(s, altered), "shader size rejected");
    altered = p;
    altered.technique++;
    check(!cd::single_native_output(s, altered), "shader technique rejected");
    auto alias = s;
    alias.inputs[0].resource = s.inputs[4].resource;
    check(!cd::single_native_output(alias, p), "input/output alias rejected");
    auto menu = s;
    auto mp = p;
    menu.technique = mp.technique = 29842;
    mp.size = 15284;
    check(!cd::single_native_output(menu, mp), "unscaled admission unchanged");
    check(cd::single_native_output(menu, mp, true), "captured menu recipe admitted for scaled runtime");
    check(cd::native_shader_index(23086, 15284, true) == cd::unknown_native_shader &&
              cd::native_shader_index(29842, 15492, true) == cd::unknown_native_shader &&
              cd::native_shader_index(29843, 15284, true) == cd::unknown_native_shader,
          "size and technique pairs remain exact");
    for (unsigned i = 0; i < 7; ++i) {
        auto bad = mp;
        bad.bindings[i].cpu_descriptor = 0;
        check(!cd::single_native_output(menu, bad, true), "menu descriptor must exist");
        bad = mp;
        bad.owners[i].object++;
        check(!cd::single_native_output(menu, bad, true), "menu owner substitution rejected");
        bad = mp;
        bad.owners[i].id++;
        check(!cd::single_native_output(menu, bad, true), "menu binding identity substitution rejected");
    }
    auto bad_menu = menu;
    bad_menu.inputs[0].resource = menu.inputs[4].resource;
    check(!cd::single_native_output(bad_menu, mp, true), "menu input/output alias rejected");
    auto bad_recipe = mp;
    bad_recipe.binding_count = 6;
    check(!cd::single_native_output(menu, bad_recipe, true), "menu missing UAV rejected");
    bad_recipe = mp;
    bad_recipe.bindings[7] = mp.bindings[6];
    bad_recipe.binding_count = 8;
    check(!cd::single_native_output(menu, bad_recipe, true), "menu duplicate UAV rejected");
    cd::NativeShaderCache cache;
    unsigned hashes{};
    std::array<std::byte, cd::native_taa_shader_bytes> gameplay_bytes{};
    std::array<std::byte, 15284> menu_bytes{};
    gameplay_bytes[0] = std::byte{1};
    menu_bytes[0] = std::byte{2};
    const auto hash = [&](std::span<const std::byte> data) -> std::string_view {
        ++hashes;
        return data[0] == std::byte{1} ? cd::native_shader_variants[0].sha : cd::native_shader_variants[1].sha;
    };
    check(!cache.matches(0, gameplay_bytes, [](auto) { return std::string_view{"wrong digest"}; }) &&
              !cache.verified[0],
          "first hash must match");
    check(!cache.matches(cd::unknown_native_shader, menu_bytes, hash) && !cache.matches(0, menu_bytes, hash) && !hashes,
          "unknown variant and wrong length never hashed");
    check(cache.matches(1, menu_bytes, hash) && cache.matches(0, gameplay_bytes, hash) &&
              cache.matches(1, menu_bytes, hash) && hashes == 2,
          "menu-gameplay-menu independently verified");
    menu_bytes.back() = std::byte{99};
    check(!cache.matches(1, menu_bytes, hash) && hashes == 2, "cached menu mutation rejected without rehash");
    check(cache.matches(0, gameplay_bytes, hash) && hashes == 2,
          "menu rejection cannot invalidate gameplay verification");
    check(cache.accepted[0] == 2 && cache.accepted[1] == 2, "variant counts only accepted invocations");
    // First rejection must keep the exact tested bytes without another read.
    auto rejection = std::make_unique<cd::ShaderRejection>();
    std::array<std::byte, cd::native_taa_shader_bytes> tested{};
    tested[15] = std::byte{73};
    unsigned reads{};
    const tb::Reader r{&reads, [](void *u, std::uintptr_t at, std::span<std::byte> out) {
                           ++*static_cast<unsigned *>(u);
                           if (at != 1234) return false;
                           std::fill(out.begin(), out.end(), std::byte{42});
                           return true;
                       }};
    rejection->capture(s, p, true, true, false, false, tested, r);
    check(rejection->present && rejection->bytes == tested.size() && rejection->captured_from_tested_bytes && !reads &&
              rejection->bytecode[15] == std::byte{73},
          "exact failed hash bytes retained");
    auto changed = s;
    changed.technique = 7;
    rejection->capture(changed, p, false, false, false, false, {}, r);
    check(rejection->source.technique == 23086 && !reads, "first rejection immutable");
    rejection = std::make_unique<cd::ShaderRejection>();
    altered = p;
    altered.size = 52;
    altered.bytecode = 1234;
    rejection->capture(s, altered, false, false, false, false, {}, r);
    check(reads == 1 && rejection->bytes == 52 && !rejection->captured_from_tested_bytes &&
              rejection->bytecode[0] == std::byte{42},
          "bounded shader copy for rejected shape");
    for (unsigned size : {0U, 51U, tb::shader_limit + 1}) {
        rejection = std::make_unique<cd::ShaderRejection>();
        altered.size = size;
        rejection->capture(s, altered, false, false, false, false, {}, r);
        check(!rejection->bytes && reads == 1, "malformed shader length never read");
    }
    rejection = std::make_unique<cd::ShaderRejection>();
    altered.size = 52;
    altered.bytecode = 9;
    rejection->capture(s, altered, false, false, false, false, {}, r);
    check(!rejection->bytes && reads == 2, "unreadable shader metadata survives");
    std::ostringstream diagnostic;
    rejection->write(diagnostic);
    check(diagnostic.str().find("\"shape_matches\":false") != std::string::npos && diagnostic.str().size() < 28000,
          "bounded rejection JSON");
    cd::ResolveTelemetry telemetry;
    for (auto outcome :
         {cd::ResolveOutcome::reconstructed, cd::ResolveOutcome::unfiltered, cd::ResolveOutcome::recovered,
          cd::ResolveOutcome::rejected, cd::ResolveOutcome::unrecoverable}) {
        unsigned native{}, replacement{};
        telemetry.begin();
        const auto result = cd::dispatch_resolve(
            true,
            [&] {
                ++replacement;
                return cd::ResolveResult{outcome};
            },
            [&] { ++native; });
        telemetry.end(result);
        check(!native && replacement == 1 && result.claimed,
              "claimed invocation never calls native, including failure");
    }
    unsigned native{}, replacement{};
    telemetry.begin();
    const auto unclaimed = cd::dispatch_resolve(
        false,
        [&] {
            ++replacement;
            return cd::ResolveResult{};
        },
        [&] { ++native; });
    telemetry.end(unclaimed);
    check(native == 1 && !replacement && !unclaimed.claimed, "unrelated work forwards exactly once");
    volatile bool throw_replacement = true;
    bool caught = false;
    try {
        cd::dispatch_resolve(
            true,
            [&]() -> cd::ResolveResult {
                if (throw_replacement) throw std::runtime_error("test");
                return {};
            },
            [&] { ++native; });
    } catch (const std::runtime_error &) {
        caught = true;
    }
    check(caught && native == 1, "exception cannot fall through into native TAA");
    check(telemetry.candidates == 6 && !telemetry.pending && telemetry.claimed == 5 &&
              telemetry.unclaimed_native == 1 && telemetry.reconstructed == 1 && telemetry.unfiltered == 1 &&
              telemetry.recovered == 1 && telemetry.rejected == 1 && telemetry.unrecoverable == 1,
          "bounded routing receipts");
    std::ostringstream report;
    telemetry.write(report);
    check(report.str().find("\"exclusive_mode\":false") != std::string::npos, "no feature-wide exclusivity claim");
    std::cout << "Native TAA recipe, alias rejection, dispatch suppression and failure routing passed\n";
}

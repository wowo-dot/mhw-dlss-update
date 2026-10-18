#include "game_options.hpp"
#include "../game_memory/layout.hpp"
#include "../astera/astera_hooks.hpp"
#include "../projection_binding/memory.hpp"
#include "../ngx_startup/startup.hpp"
#include <windows.h>
#include <xmmintrin.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <ostream>

namespace mhw::continuous_dlaa::game_options {
namespace {
namespace pb = projection_binding;
// The game's functions (clean image; gates in install()).
using AaApply = std::uintptr_t (*)(unsigned char); // 0x43dcb0: the anti-aliasing option (TAA under its DLSS)
using RsApply = __m128 (*)(char);                  // 0x43eeb0: resolution scaling (High under its DLSS); the scale
using AaSetter = void (*)(std::uintptr_t, char);   // 0x2287a00: the TAA/FXAA bytes at table +0xe9e4
using IniRead = char (*)(std::uintptr_t);          // 0x4413e0, 0x442120: the saved choice
using RowEnabled = std::uint64_t (*)(int, unsigned char);    // 0x53f580: a row takes input
using PageRefresh = std::uintptr_t (*)(std::uintptr_t, int); // 0x53f440: builds the visible rows, greys the locked ones
using RowBuild = std::uintptr_t (*)(std::uintptr_t, char, unsigned, int, std::uintptr_t, unsigned, unsigned,
                                    std::uintptr_t);        // 0x53f190
using RestoreDefaults = std::uintptr_t (*)(std::uintptr_t); // 0x540720: copies the defaults, except locked rows
using Grey = std::uintptr_t (*)(std::uintptr_t, int);       // 0xb72f00: a row's greyed look (9)
// FidelityFX CAS (v0.30.1): 0x43de50's argument, as 0x441520 returns the
// saved choice packed in a register: on, then the sharpness.
struct CasValue {
    unsigned char on;
    float sharpness;
};
static_assert(sizeof(CasValue) == 8 && offsetof(CasValue, sharpness) == 4, "the game's CAS value");
using CasApply = __m128 (*)(const CasValue *);     // 0x43de50: CAS (Off under this mod's DLSS); re-applies the scaling
using CasRead = std::uint64_t (*)(std::uintptr_t); // 0x441520: the saved CAS, packed as CasValue
using RowAvailable = char (*)(std::uintptr_t, int *, char); // 0xeeae80: a display menu row takes input
// The player's TAA and FXAA in sScene, the renderer's variable resolution
// byte (0x229bf30), and the saved settings' file and section: layout.hpp.
// uGUIGraphicsOption, as its gated functions use it: 13 visible rows from
// +11008, 112 bytes each, with the item id at +8 and the row's GUI object at
// +40; the menu's values of the two rows; the list and its first row.
constexpr std::uintptr_t rows_offset = 11008, row_size = 112, row_item = 8, row_help = 24, row_object = 40,
                         menu_rs = 12520, menu_aa = 12525, menu_list = 10992, list_top = 868;
// The display menu's row descriptor, as 0xeeae80 reads it: its kind and index.
constexpr std::uintptr_t display_row_kind = 0, display_row_index = 20;
constexpr unsigned rows = 13, greyed_look = 9;

std::uintptr_t image{};
std::string refusal;
AaApply aa_original{};
RsApply rs_original{};
AaSetter aa_setter{};
Grey grey{};
RowEnabled row_original{};
PageRefresh page_original{};
RowBuild build_original{};
RestoreDefaults defaults_original{};
CasApply cas_original{};
RowAvailable available_original{};
// v0.30.0: al.hooks under AL (astera::OwnedHook). Patterns: each function's
// first 16 original bytes (clean image, checked against the gates in install()).
struct HookSpec {
    unsigned rva;
    const char *name, *pattern;
};
constexpr HookSpec hook_specs[]{
    {0x43dcb0, "mhw_dlss Options:AaApply", "40 53 48 83 EC 50 0F B6 D9 E8 32 55 00 00 84 C0"},
    {0x43eeb0, "mhw_dlss Options:RsApply", "40 53 48 83 EC 60 0F 29 74 24 50 0F B6 D9 0F 29"},
    {0x53f580, "mhw_dlss Options:RowEnabled", "48 89 5C 24 08 57 48 83 EC 20 0F B6 FA B3 01 83"},
    {0x53f440, "mhw_dlss Options:PageRefresh", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48"},
    {0x53f190, "mhw_dlss Options:RowBuild", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48"},
    {0x540720, "mhw_dlss Options:RestoreDefaults", "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55"},
    {0x43de50, "mhw_dlss Options:CasApply", "40 57 48 81 EC 20 01 00 00 0F B6 11 48 8B 05 1D"},
    {0xeeae80, "mhw_dlss Options:RowAvailable", "40 53 48 83 EC 20 48 8B DA 48 85 C9 74 50 44 8B"}};
static_assert(
    [] {
        for (const auto &h : hook_specs)
            if (!astera::valid_pattern<16>(h.pattern)) return false;
        return true;
    }(),
    "each game options pattern is its function's first 16 bytes");
std::array<astera::OwnedHook, std::size(hook_specs)> hooks{};
std::atomic<bool> taken_now{};
// The player's choices: the saved ones at install, then every value the game
// applies (its menu, presets, start-up), except restore defaults while taken.
std::atomic<int> player_aa{-1}, player_rs{-1};
int saved_aa{-1}, saved_rs{-1};
// CAS packed as the game's 0x441520 returns it (pack()); unknown until read.
constexpr std::uint64_t cas_unknown = ~0ULL;
std::atomic<std::uint64_t> player_cas{cas_unknown};
std::uint64_t saved_cas{cas_unknown};
std::atomic<std::uint64_t> aa_applies{}, aa_forced{}, rs_applies{}, rs_forced{}, rows_locked{}, rows_greyed{}, helps{},
    defaults_kept{}, take_overs{}, give_backs{}, call_faults{}, cas_applies{}, cas_forced{}, display_rows_locked{};
thread_local bool restoring_defaults{};

template <class T> bool write_value(std::uintptr_t at, T value) noexcept {
    __try {
        *reinterpret_cast<volatile T *>(at) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
std::uint64_t pack(const CasValue &v) noexcept {
    std::uint32_t bits{};
    std::memcpy(&bits, &v.sharpness, sizeof bits);
    return v.on | std::uint64_t{bits} << 32;
}
CasValue unpack(std::uint64_t packed) noexcept {
    CasValue v{static_cast<unsigned char>(packed & 0xff), 0.F};
    const auto bits = static_cast<std::uint32_t>(packed >> 32);
    std::memcpy(&v.sharpness, &bits, sizeof bits);
    return v;
}
// Worker calls into the game, as its graphics menu makes them.
bool set_aa(unsigned char value) noexcept {
    std::uintptr_t table{};
    if (!pb::read(image + layout::image::scene, table) || !table) return false;
    __try {
        aa_setter(table, static_cast<char>(value));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool apply_rs(unsigned char value) noexcept {
    __try {
        rs_original(static_cast<char>(value));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool apply_cas(const CasValue &value) noexcept {
    __try {
        cas_original(&value);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// CAS Off with the player's sharpness (else the renderer's current one).
CasValue cas_off() noexcept {
    if (const auto cas = player_cas.load(); cas != cas_unknown) return {0, unpack(cas).sharpness};
    std::uintptr_t renderer{};
    float sharpness{};
    if (pb::read(image + layout::image::renderer, renderer) && renderer)
        pb::read(renderer + layout::renderer::cas_sharpness, sharpness);
    return {0, sharpness};
}
std::uint64_t saved_cas_value() noexcept {
    __try {
        // The upper bytes of the flag's dword are padding (zero).
        return reinterpret_cast<CasRead>(image + 0x441520)(image + layout::image::graphics_option_ini) &
               0xffffffff000000ffULL;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return cas_unknown;
    }
}
int saved(std::uintptr_t rva, unsigned char values) noexcept {
    char value{};
    __try {
        value = reinterpret_cast<IniRead>(image + rva)(image + layout::image::graphics_option_ini);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
    return static_cast<unsigned char>(value) < values ? static_cast<unsigned char>(value) : -1;
}

std::uintptr_t aa_apply(unsigned char value) {
    ++aa_applies;
    if (value < anti_aliasing_values && !restoring_defaults) player_aa = value;
    if (taken_now.load()) {
        ++aa_forced;
        return aa_original(anti_aliasing_off);
    }
    return aa_original(value);
}
__m128 rs_apply(char value) {
    ++rs_applies;
    const auto v = static_cast<unsigned char>(value);
    if (v < resolution_scaling_values && !restoring_defaults) player_rs = v;
    if (taken_now.load()) {
        ++rs_forced;
        return rs_original(static_cast<char>(resolution_scaling_high));
    }
    return rs_original(value);
}
// Every CAS the game applies (its display menu, opening it) is the player's;
// under this mod's DLSS it applies as Off.
__m128 cas_apply(const CasValue *value) {
    ++cas_applies;
    if (!value) return cas_original(value);
    player_cas = pack(*value);
    if (taken_now.load()) {
        ++cas_forced;
        const CasValue off{0, value->sharpness};
        return cas_original(&off);
    }
    return cas_original(value);
}
// The display menu's rows: CAS and its sharpness take no input while taken,
// and the callers grey a row that takes none (0x1e8d200, 0x1e82170).
char row_available(std::uintptr_t row, int *reason, char mode) {
    unsigned kind{};
    unsigned char index{};
    if (taken_now.load() && row && pb::read(row + display_row_kind, kind) && pb::read(row + display_row_index, index) &&
        locked_display_row(kind, index)) {
        ++display_rows_locked;
        return 0;
    }
    return available_original(row, reason, mode);
}
std::uint64_t row_enabled(int mode, unsigned char item) {
    if (taken_now.load() && locked_row(item)) {
        ++rows_locked;
        return 0;
    }
    return row_original(mode, item);
}
std::uintptr_t page_refresh(std::uintptr_t menu, int top) {
    const auto result = page_original(menu, top);
    // The page's own pass greys the rows the game's DLSS locks; the same
    // rows, after it.
    if (taken_now.load())
        for (unsigned i = 0; i < rows; ++i) {
            const auto row = menu + rows_offset + row_size * i;
            unsigned char item{};
            std::uintptr_t object{};
            if (!pb::read(row + row_item, item) || !locked_row(item) || !pb::read(row + row_object, object) || !object)
                continue;
            grey(object, greyed_look);
            ++rows_greyed;
        }
    return result;
}
std::uintptr_t row_build(std::uintptr_t row, char item, unsigned label, int help, std::uintptr_t values, unsigned value,
                         unsigned count, std::uintptr_t callback) {
    // The game's own help for the row while NVIDIA DLSS is on. With CAS saved
    // on, the game's build swaps in its CAS help afterwards (row +24); this
    // mod's DLSS is in effect, not CAS, so its help again.
    const bool locked = taken_now.load() && locked_row(static_cast<unsigned char>(item));
    if (locked) {
        ++helps;
        help = locked_help(static_cast<unsigned char>(item), help);
    }
    const auto result = build_original(row, item, label, help, values, value, count, callback);
    if (locked) write_value(row + row_help, help);
    return result;
}
std::uintptr_t restore_defaults(std::uintptr_t menu) {
    if (!taken_now.load()) return defaults_original(menu);
    // As for the game's DLSS: the locked rows keep the player's values, and
    // what the defaults apply meanwhile is not the player's choice.
    unsigned char rs{}, aa{};
    const bool read = pb::read(menu + menu_rs, rs) && pb::read(menu + menu_aa, aa);
    restoring_defaults = true;
    const auto result = defaults_original(menu);
    restoring_defaults = false;
    std::uintptr_t list{};
    unsigned top{};
    if (read && write_value(menu + menu_rs, rs) && write_value(menu + menu_aa, aa) &&
        pb::read(menu + menu_list, list) && list && pb::read(list + list_top, top)) {
        ++defaults_kept;
        page_refresh(menu, static_cast<int>(top));
    }
    return result;
}
} // namespace

bool install(std::uintptr_t host, std::string &reason) {
    if (image) {
        reason = "game options already installed";
        return false;
    }
    struct Gate {
        unsigned rva, bytes;
        const char *sha;
    };
    constexpr Gate gates[]{{0x43dcb0, 354, "4f4ccf7d6e736233fc0c47ce082ab05593116b183ec2884537633b7ba8c6afdd"},
                           {0x43eeb0, 332, "d2dafa94a77e203e1156c03bd2dd8564450d86804d5644218d51e20949fa0b8f"},
                           {0x2287a00, 66, "fd224da012d8cd5cd2175b7f67769b5cf6b5b532bfb0b9dfcf43bcd070f38494"},
                           // TAA2's and FXAA's per-frame enable from the table bytes (vtable slot 10).
                           {0x2393260, 247, "a0f23cc4334b1e0e03b98c99894004f23cc0e6ef82d469ace598789081fc9e46"},
                           {0x2394140, 309, "0ba352e5c5af5fb35e71744e267bdac8bec9d92de9d9f49ddbb1830cf46cc18c"},
                           {0x4413e0, 169, "24516d839a3428bd0b27df3f98c9edff5b9349564ba4c2eabb0f0587a50a6327"},
                           {0x442120, 185, "d9e6710d5a91105f22a09054f9e35e006c8cfe6a60c8b6ff79c2b92325643068"},
                           {0x53f580, 124, "844b65e650c6b9c72ebd7d6f96a1bbf532dd40c38632757d7da3dbdda9486835"},
                           {0x53f440, 316, "8299ed6141b50713d145dd033628c787c4300965d409e3225ea91ddb95735306"},
                           {0x53f190, 680, "14444dd15728755ab84854f7b91b32bff3ae0101f959021e5b3ff06658785a4b"},
                           {0x540720, 824, "6557fffa6347d6e98d060f7a25605c018ee473e301a8acd15028952a36c637d6"},
                           {0xb72f00, 17, "99fb83c8c457ec4bab9bf6273293cc99dabd28e9be4c289e92c54b57426538f4"},
                           // FidelityFX CAS (v0.30.1): its apply, the saved
                           // choice, the content scale it sets, and the
                           // display menu's row check.
                           {0x43de50, 243, "9cf2e259bc0b7645a46bbc4a3ff7be866e2d7ee75c16f8a9fe86db894ec455cf"},
                           {0x441520, 245, "c57cfde37ec28e3efdce6ff9d6c046922dfff64482d7ff9dd500a26685dc7f92"},
                           {0x229bd20, 67, "3e57d8383f4151288f0b061f35f7683d05bcf1b38618f3d0e4dcde564b41358b"},
                           {0xeeae80, 520, "3b64ba6206055f74b82e90901c6573c14a040b730bab041df1c143d5de2627b7"}};
    for (const auto &g : gates)
        if (!pb::verify_code(host, g.rva, g.bytes, g.sha)) {
            reason = "game options code gate " + std::to_string(g.rva);
            refusal = reason;
            return false;
        }
    image = host;
    aa_setter = reinterpret_cast<AaSetter>(host + 0x2287a00);
    grey = reinterpret_cast<Grey>(host + 0xb72f00);
    saved_aa = saved(0x4413e0, anti_aliasing_values);
    saved_rs = saved(0x442120, resolution_scaling_values);
    player_aa = saved_aa;
    player_rs = saved_rs;
    saved_cas = saved_cas_value();
    player_cas = saved_cas;
    // Pass-through until take_over(), live as each goes in: every detour
    // continues with its own typed original, written before it can run.
    const std::array<void *, hooks.size()> detours{
        reinterpret_cast<void *>(aa_apply),    reinterpret_cast<void *>(rs_apply),
        reinterpret_cast<void *>(row_enabled), reinterpret_cast<void *>(page_refresh),
        reinterpret_cast<void *>(row_build),   reinterpret_cast<void *>(restore_defaults),
        reinterpret_cast<void *>(cas_apply),   reinterpret_cast<void *>(row_available)};
    const std::array<void **, hooks.size()> originals{
        reinterpret_cast<void **>(&aa_original),    reinterpret_cast<void **>(&rs_original),
        reinterpret_cast<void **>(&row_original),   reinterpret_cast<void **>(&page_original),
        reinterpret_cast<void **>(&build_original), reinterpret_cast<void **>(&defaults_original),
        reinterpret_cast<void **>(&cas_original),   reinterpret_cast<void **>(&available_original)};
    bool live = true;
    for (unsigned i = 0; i < hooks.size() && live; ++i) {
        hooks[i].name = hook_specs[i].name;
        hooks[i].pattern = hook_specs[i].pattern;
        hooks[i].target = reinterpret_cast<void *>(host + hook_specs[i].rva);
        live = astera::hook_owned(hooks[i], detours[i], originals[i]);
    }
    if (live) return true;
    for (auto &h : hooks) astera::unhook_owned(h);
    image = 0;
    reason = "game options hooks";
    refusal = reason;
    return false;
}
void take_over() noexcept {
    if (!image || taken_now.exchange(true)) return;
    ++take_overs;
    // The units follow the table bytes on their next update (0x2393260,
    // 0x2394140); the game's apply also walks the unit list, which stays on
    // the game's own thread.
    if (!set_aa(anti_aliasing_off)) ++call_faults;
    // CAS's apply re-applies the resolution scaling (High: taken); High last.
    if (!apply_cas(cas_off())) ++call_faults;
    if (!apply_rs(resolution_scaling_high)) ++call_faults;
}
void give_back() noexcept {
    if (!image || !taken_now.exchange(false)) return;
    ++give_backs;
    // Read before CAS's apply re-applies (and records) the saved scaling.
    const int aa = player_aa.load(), rs = player_rs.load();
    const auto cas = player_cas.load();
    if (aa >= 0 && !set_aa(static_cast<unsigned char>(aa))) ++call_faults;
    if (cas != cas_unknown && !apply_cas(unpack(cas))) ++call_faults;
    if (rs >= 0 && !apply_rs(static_cast<unsigned char>(rs))) ++call_faults;
}
bool taken() noexcept {
    return taken_now.load();
}
void write(std::ostream &out) {
    std::uintptr_t table{}, renderer{};
    unsigned char taa{}, fxaa{}, variable{}, cas_on{};
    float scale{}, cas_sharpness{}, cas_scale{};
    if (image && pb::read(image + layout::image::scene, table) && table) {
        pb::read(table + layout::scene::taa, taa);
        pb::read(table + layout::scene::fxaa, fxaa);
    }
    if (image && pb::read(image + layout::image::renderer, renderer) && renderer) {
        pb::read(renderer + layout::renderer::variable_resolution, variable);
        pb::read(renderer + layout::renderer::scale_desired, scale);
        pb::read(renderer + layout::renderer::cas, cas_on);
        pb::read(renderer + layout::renderer::cas_sharpness, cas_sharpness);
        pb::read(renderer + layout::renderer::cas_scale, cas_scale);
    }
    const auto cas_text = [](std::uint64_t packed) {
        if (packed == cas_unknown) return std::string("null");
        const auto v = unpack(packed);
        return '[' + std::to_string(unsigned(v.on)) + ',' + std::to_string(v.sharpness) + ']';
    };
    // The game's own DLSS 1 (0x23842a0 at cD3DRender::initForD3D12): its NGX
    // context at backend +0x42700, or why it has none (0x51c6c28: 1 NGX
    // init, 2 SuperSampling, 3 ImageSignalProcessing unavailable), and this
    // mod's decline of it (ngx_startup::game_dlss(): 2 none made, 3 declined).
    std::uintptr_t backend{}, context{};
    unsigned game_ngx{};
    if (image && pb::read(image + layout::image::d3d_render, backend) && backend)
        pb::read(backend + layout::d3d_render::game_dlss, context);
    if (image) pb::read(image + layout::image::game_dlss_state, game_ngx);
    out << "{\"installed\":" << (image != 0) << ",\"refusal\":\"" << refusal << "\",\"taken\":" << taken_now.load()
        << ",\"saved\":[" << saved_aa << ',' << saved_rs << "],\"player\":[" << player_aa.load() << ','
        << player_rs.load() << "],\"table\":[" << unsigned(taa) << ',' << unsigned(fxaa)
        << "],\"variable\":" << unsigned(variable) << ",\"scale\":" << scale << ",\"take_overs\":" << take_overs.load()
        << ",\"give_backs\":" << give_backs.load() << ",\"call_faults\":" << call_faults.load() << ",\"aa\":["
        << aa_applies.load() << ',' << aa_forced.load() << "],\"rs\":[" << rs_applies.load() << ',' << rs_forced.load()
        << "],\"rows\":{\"locked\":" << rows_locked.load() << ",\"greyed\":" << rows_greyed.load()
        << ",\"helps\":" << helps.load() << ",\"defaults_kept\":" << defaults_kept.load()
        << ",\"display_locked\":" << display_rows_locked.load() << "},\"cas\":{\"saved\":" << cas_text(saved_cas)
        << ",\"player\":" << cas_text(player_cas.load()) << ",\"renderer\":[" << unsigned(cas_on) << ','
        << cas_sharpness << ',' << cas_scale << "],\"applies\":" << cas_applies.load()
        << ",\"forced\":" << cas_forced.load() << "},\"game_dlss\":{\"context\":" << (context != 0)
        << ",\"refusal\":" << game_ngx << ",\"declined\":" << ngx_startup::game_dlss() << "}}";
}
} // namespace mhw::continuous_dlaa::game_options

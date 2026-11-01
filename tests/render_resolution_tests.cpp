// Production callback and forwarding ASM against owned native-shaped memory.
#include "../src/render_resolution/runtime.cpp"
#include "../src/dlss_sdk/geometry.hpp"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
namespace rr = mhw::render_resolution;
void check(bool ok, const char *why) {
    if (!ok) {
        std::cerr << why << '\n';
        std::exit(1);
    }
}
template <class T> void put(std::uintptr_t at, T value) {
    std::memcpy(reinterpret_cast<void *>(at), &value, sizeof(value));
}
template <class T> T get(std::uintptr_t at) {
    T v{};
    check(rr::pb::read(at, v), "fixture read");
    return v;
}
struct Block {
    std::uintptr_t p{};
    explicit Block(std::size_t size)
        : p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))) {
        check(p != 0, "fixture allocation");
    }
    ~Block() { VirtualFree(reinterpret_cast<void *>(p), 0, MEM_RELEASE); }
};
unsigned action{}, native_calls{};
float native_expected = 1.F;
constexpr std::uint64_t answer = 0x987654321abcdef0ULL;
std::uint64_t native_update(std::uintptr_t r, std::uintptr_t b, std::uintptr_t c, std::uintptr_t d) {
    check(r == rr::renderer && b == 2 && c == 3 && d == 4, "forwarded arguments");
    check(get<float>(r + 0x1f0) == native_expected, "native updater sees its own request");
    ++native_calls;
    if (action == 1) RaiseException(0xe0424242, 0, 0, nullptr);
    if (action == 2) {
        put(r + 0x1f0, .75F);
        native_expected = .75F;
    }
    return answer;
}
void native_setter(std::uintptr_t r, float scale) {
    if (action == 3) RaiseException(0xe0424242, 0, 0, nullptr);
    // The game's setter aligns down and nothing more; the plan asks it for a
    // scale that lands on a step at or above half.
    const auto basis = get<unsigned>(r + 0x198);
    const float written = static_cast<float>(rr::native_width(basis, scale)) / static_cast<float>(basis);
    check(written >= .5F && written <= 1.F, "fixture setter scale");
    put(r + 0x1f0, written);
}
void step() {
    rr::pb::Frame f{};
    f.caller = rr::expected_return;
    f.gpr = {0, rr::renderer, 2, 3, 4, 5, 6};
    rr::dispatch(&f);
    check(f.gpr[0] == answer, "native return register preserved");
    const float desired = get<float>(rr::renderer + 0x1f0);
    const auto v = get<std::uintptr_t>(rr::renderer + 0x1f450);
    put(rr::renderer + 0x1f8, get<float>(rr::renderer + 0x1f4));
    put(rr::renderer + 0x1f4, desired);
    put(rr::renderer + 0x208, desired);
    put(v + 0x6b0, desired);
    put(v + 0x6b8, desired);
    put(rr::renderer + 0x1f210, get<std::uint64_t>(rr::renderer + 0x1f210) + 1);
}
void exception_step() {
    bool caught = false;
    __try {
        step();
    } __except (GetExceptionCode() == 0xe0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        caught = true;
    }
    check(caught, "native exception propagated");
}
int main() {
    const auto p = rr::plan({2560, 1440}, .5F);
    check(p.valid && p.render == rr::Extent{1280, 720} && p.quantized == .5F, "half-size plan");
    const auto q = rr::plan({2560, 1440}, 2.F / 3.F);
    check(q.valid && q.render == rr::Extent{1696, 954}, "native 16-pixel quantization");
    // The DLSS presets: Balanced 58% quantizes to 1472 (0.575), DLAA is the scene.
    check(rr::plan({2560, 1440}, .58F).render == rr::Extent{1472, 828}, "balanced plan");
    check(rr::plan({2560, 1440}, 1.F).render == rr::Extent{2560, 1440} && rr::plan({2560, 1440}, 1.F).quantized == 1.F,
          "full-size plan");
    check(rr::plan({1920, 1080}, 2.F / 3.F).render == rr::Extent{1280, 720}, "quality at 1080p");
    // Every preset on common displays (and 21:9 pillarboxed in a 32:9 window):
    // the game's quantized extent is one scale per axis, as DLSS and the
    // shader adaptations require.
    for (const auto d :
         {rr::Extent{1920, 1080}, rr::Extent{2560, 1440}, rr::Extent{3440, 1440}, rr::Extent{3840, 2160},
          rr::Extent{5120, 1440}, rr::Extent{2560, 1080}, rr::Extent{1600, 900}, rr::Extent{1366, 768},
          rr::Extent{3413, 1440}, rr::Extent{5120, 2880}, rr::Extent{7680, 4320}, rr::Extent{10240, 2880}})
        for (const auto m :
             {mhw::dlss_sdk::ReconstructionMode::performance, mhw::dlss_sdk::ReconstructionMode::balanced,
              mhw::dlss_sdk::ReconstructionMode::quality, mhw::dlss_sdk::ReconstructionMode::dlaa}) {
            const auto planned = rr::plan(d, d.width == 3413 ? 5120U : d.width, mhw::dlss_sdk::requested_scale(m));
            check(planned.valid, "every preset plans on common displays");
            check(mhw::dlss_sdk::single_scale(planned.render.width, planned.render.height, d.width, d.height),
                  "preset plan is one scale");
        }
    check(rr::plan({3840, 2160}, .5F).render == rr::Extent{1920, 1080}, "4K geometry without allocation claim");
    check(rr::plan({5120, 1440}, .5F).render == rr::Extent{2560, 720}, "ultrawide geometry");
    check(rr::plan({7680, 4320}, .5F).render == rr::Extent{3840, 2160} &&
              rr::plan({16384, 16384}, 1.F).render == rr::Extent{16384, 16384},
          "8K and the largest D3D12 side (v0.24.0)");
    // 21:9 pillarboxed in a 5120x1440 window: the scale quantizes against the
    // window (exactly 0.5), and the odd scene width truncates (game-20).
    const auto pillar = rr::plan({3413, 1440}, 5120, .5F);
    check(pillar.valid && pillar.quantized == .5F && pillar.render == rr::Extent{1706, 720},
          "pillarbox plan against the window basis");
    // 0.40.1 (a 3440x1440 report): where half the basis is not a 16-pixel step,
    // 50% aligned down below half and Performance never armed. The plan asks
    // the setter for the next step up; asked again, it keeps that scale.
    const auto wide = rr::plan({3440, 1440}, .5F);
    check(wide.valid && wide.render == rr::Extent{1728, 723} && wide.quantized == 1728.F / 3440.F &&
              rr::native_width(3440, wide.requested) == 1728 &&
              rr::plan({3440, 1440}, wide.requested).quantized == wide.quantized,
          "21:9 Performance steps up to the next 16 pixels");
    check(rr::plan({1366, 768}, .5F).render == rr::Extent{688, 386}, "1366 Performance steps up");
    check(rr::plan({3413, 1440}, .5F).quantized == 1712.F / 3413.F, "an odd scene alone steps up too");
    check(rr::plan({2560, 1080}, 2560, .5F).render == rr::Extent{1280, 540}, "letterbox plan");
    check(!rr::plan({3413, 1440}, 3412, .5F).valid, "basis narrower than the scene");
    // Every basis width: the setter's own float arithmetic lands Performance on
    // the 16-pixel step at or above half.
    for (unsigned w = 64; w <= rr::max_side; ++w) {
        const auto planned = rr::plan({w, 64}, .5F);
        const unsigned step = ((w + 1) / 2 + 15) & ~15U;
        check(planned.valid && planned.requested <= 1.F && rr::native_width(w, planned.requested) == step &&
                  planned.quantized == static_cast<float>(step) / static_cast<float>(w),
              "every width plans Performance");
    }
    for (float scale :
         {0.F, .49F, 1.01F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        check(!rr::plan({2560, 1440}, scale).valid, "invalid scale");
    check(!rr::plan({0, 1440}, .5F).valid && !rr::plan({16385, 1440}, .5F).valid && !rr::plan({2560, 16385}, .5F).valid,
          "invalid display extent");
    check(rr::matching_basis({2560, 1440}, 1, {2560, 1440}, {2560, 1440}), "matching native override dimensions");
    check(!rr::matching_basis({2560, 1440}, 1, {1920, 1080}, {2560, 1440}) &&
              !rr::matching_basis({2560, 1440}, 2, {2560, 1440}, {2560, 1440}),
          "reject different or unknown sizing basis");
    // 21:9 in a 2560x1440 window (game-17) and pillarboxed in 5120x1440.
    check(rr::matching_basis({2560, 1080}, 1, {2560, 1440}, {2560, 1440}) &&
              rr::matching_basis({3440, 1440}, 1, {5120, 1440}, {5120, 1440}),
          "aspect basis is the window");
    check(!rr::matching_basis({2560, 1080}, 0, {2560, 1440}, {2560, 1440}) &&
              !rr::matching_basis({2560, 1080}, 1, {2560, 1440}, {1920, 1080}) &&
              !rr::matching_basis({2560, 1600}, 1, {2560, 1440}, {2560, 1440}),
          "aspect basis needs the flag, the window size and a scene inside it");
    rr::Ownership owner;
    float restore{};
    check(!owner.before(1.F, restore) && restore == 1.F, "unowned request untouched");
    owner.after(1.F, .5F);
    check(owner.before(.5F, restore) && restore == 1.F && !owner.owned, "restore ours before native policy");
    owner.after(1.F, .5F);
    check(!owner.before(.75F, restore) && restore == .75F && !owner.owned, "foreign request wins");
    Block image(0x5300000), renderer(0x800000), view(0x1000), backend(0x43000), config(0x3000), depth(0x1000),
        texture(0x1000);
    rr::image = image.p;
    rr::renderer = renderer.p;
    rr::expected_return = image.p + 0x22982e7;
    rr::discovered.backend = backend.p;
    rr::target = p;
    rr::setter = native_setter;
    rr::original = reinterpret_cast<void *>(native_update);
    rr::requested = true;
    put(image.p + 0x51c4480, renderer.p);
    put(image.p + 0x51cc080, static_cast<unsigned char>(1));
    put(image.p + 0x51c5320, config.p);
    put(renderer.p + 0x78, backend.p);
    put(renderer.p + 0x1f450, view.p);
    put(renderer.p + 0x198, 2560U);
    put(renderer.p + 0x19c, 1440U);
    put(renderer.p + 0x1f210, std::uint64_t{1});
    for (auto offset : {0x1f0, 0x1f4, 0x1f8, 0x200, 0x204, 0x208}) put(renderer.p + offset, 1.F);
    for (auto offset : {0x6b0, 0x6b4, 0x6b8}) put(view.p + offset, 1.F);
    put(view.p + 0x628, depth.p);
    put(depth.p + 0xa0, texture.p);
    put(texture.p + 0x88, 2560U | (1440U << 16));
    rr::Snapshot captured{};
    check(rr::snapshot(captured, rr::target) && captured.width_override == 0 && captured.basis_width == 2560,
          "display width route admitted");
    check(rr::planned_view_scale(rr::target, .5F) && !rr::planned_view_scale(rr::target, .49F) &&
              rr::truncated_ratio(rr::target) == .5F,
          "2:1 scenes plan exactly half");
    put(config.p + 0x2cda, static_cast<unsigned char>(1));
    put(config.p + 0x2cd0, 2560U);
    put(config.p + 0x2cd4, 1440U);
    check(rr::snapshot(captured, rr::target) && captured.width_override == 1 && captured.basis_width == 2560,
          "live-shaped config width route admitted");
    put(config.p + 0x2cd0, 1920U);
    check(!rr::snapshot(captured, rr::target), "reject alternate width mismatch");
    put(config.p + 0x2cd0, 2560U);
    put(config.p + 0x2cd4, 1080U);
    check(!rr::snapshot(captured, rr::target), "reject alternate height mismatch");
    put(config.p + 0x2cd4, 1440U);
    put(config.p + 0x2cda, static_cast<unsigned char>(2));
    check(!rr::snapshot(captured, rr::target), "reject unknown override flag");
    put(config.p + 0x2cda, static_cast<unsigned char>(1));
    { // 21:9 pillarboxed in a 5120x1440 window: a view holding the truncated
        // ratio 1706/3413 is admitted (game-22); anything lower is not.
        const auto saved = rr::target;
        rr::target = pillar;
        rr::discovered.window_width = 5120;
        rr::discovered.window_height = 1440;
        put(renderer.p + 0x198, 3413U);
        put(config.p + 0x2cd0, 5120U);
        put(view.p + 0x6b0, 1706.F / 3413.F);
        check(rr::snapshot(captured, rr::target) && captured.basis_width == 5120, "truncated view ratio admitted");
        check(rr::planned_view_scale(rr::target, 1706.F / 3413.F) && rr::planned_view_scale(rr::target, .5F) &&
                  !rr::planned_view_scale(rr::target, 1705.F / 3413.F),
              "truncated ratio is the planned view scale");
        put(view.p + 0x6b0, 1705.F / 3413.F);
        check(!rr::snapshot(captured, rr::target) && captured.check == 17,
              "view scale below the truncated ratio refused");
        rr::target = p;
        check(!rr::snapshot(captured, rr::target) && captured.check == 15, "pillarbox snapshot needs its own plan");
        put(view.p + 0x6b0, 1.F);
        put(renderer.p + 0x198, 2560U);
        put(config.p + 0x2cd0, 2560U);
        rr::target = saved;
        rr::discovered.window_width = rr::discovered.window_height = 0;
        check(rr::snapshot(captured, rr::target), "16:9 snapshot restored");
    }
    { // 0.40.1 (xvinnie-1 runs): 16:9 pillarboxed in a 3440x1440 window plans
        // Quality at 2288/3440. After the game remakes its buffers (frame
        // generation's chain switch) the view holds 1702/2560 in its scale and
        // its combined scale; the stamp takes both, and nothing else.
        const auto saved = rr::target;
        rr::target = rr::plan({2560, 1440}, 3440, 2.F / 3.F);
        rr::discovered.window_width = 3440;
        rr::discovered.window_height = 1440;
        put(config.p + 0x2cd0, 3440U);
        const float planned = rr::target.quantized, ratio = 1702.F / 2560.F;
        check(planned == 2288.F / 3440.F && rr::target.render == rr::Extent{1702, 957} &&
                  rr::truncated_ratio(rr::target) == ratio,
              "3440 pillarbox Quality plan");
        for (auto offset : {0x1f0, 0x1f4, 0x208}) put(renderer.p + offset, planned);
        put(view.p + 0x6b0, ratio);
        put(view.p + 0x6b8, ratio);
        rr::InputStamp stamp{};
        check(rr::input_stamp(view.p, stamp) && stamp.render_width == 1702 && stamp.render_height == 957,
              "a view holding the ratio in both scales is stamped");
        put(view.p + 0x6b8, .66F);
        check(!rr::input_stamp(view.p, stamp) && rr::stamp_refusal.why == 8, "another combined scale is refused");
        for (auto offset : {0x1f0, 0x1f4, 0x208}) put(renderer.p + offset, 1.F);
        for (auto offset : {0x6b0, 0x6b8}) put(view.p + offset, 1.F);
        put(config.p + 0x2cd0, 2560U);
        rr::target = saved;
        rr::discovered.window_width = rr::discovered.window_height = 0;
        rr::stamp_refusal = {};
        check(rr::snapshot(captured, rr::target), "16:9 snapshot restored after 3440");
    }
    using Call = std::uint64_t (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t);
    check(reinterpret_cast<Call>(mhw_resolution_entry)(renderer.p, 2, 3, 4) == answer,
          "production ASM forwards unmatched caller");
    check(rr::visits == 0 && !rr::ownership.owned, "unqualified caller does not acquire scale");
    for (unsigned i = 0; i < 32; ++i) step();
    check(rr::overrides == 0 && rr::baseline.applied == 1.F, "baseline has no writes");
    for (unsigned i = 0; i < 100; ++i) step();
    check(rr::overrides == 100 && rr::pre_restores == 99 && rr::scaled_observed == 99 && rr::ownership.owned,
          "continuous override preserves native cached request");
    rr::InputStamp stamp{};
    check(rr::active_ready() && rr::input_stamp(view.p, stamp) && stamp.render_width == 1280 &&
              stamp.render_height == 720 && stamp.active_scale == .5F && stamp.content_scale == 1.F,
          "temporal capture sees applied scale");
    check(!rr::input_stamp(view.p + 8, stamp), "temporal stamp rejects different view");
    check(rr::stamp_refusal.why == 4 && rr::stamp_refusal.view == view.p && rr::stamp_refusal.asked == view.p + 8 &&
              rr::stamp_refusal.count == 1,
          "a refused stamp records why (0.40.1)");
    { // Live preset change: the next visit writes the new plan; stamps close
        // until the game shows it applied, then follow it.
        rr::basis_width = 2560;
        std::string why;
        check(!rr::retarget(.4F, why) && why == "extent plan" && rr::target.quantized == .5F,
              "an invalid retarget keeps the plan");
        check(rr::retarget(2.F / 3.F, why) && rr::target.render == rr::Extent{1696, 954} && rr::applied_visits() == 0,
              "retarget plans Quality");
        check(!rr::active_ready() && !rr::input_stamp(view.p, stamp), "old stamps close at a retarget");
        const auto before = rr::overrides;
        step();
        check(rr::overrides == before + 1 && get<float>(renderer.p + 0x1f0) == q.quantized && rr::applied_visits() == 0,
              "the next visit writes the new scale");
        step();
        check(rr::applied_visits() == 1 && rr::fault == 0, "the visit after observes it applied");
        check(rr::input_stamp(view.p, stamp) && stamp.render_width == 1696 && stamp.render_height == 954 &&
                  stamp.active_scale == q.quantized,
              "stamps follow the new plan");
        check(rr::retarget(.5F, why), "back to Performance");
        step();
        step();
        check(rr::applied_visits() == 1 && rr::input_stamp(view.p, stamp) && stamp.render_width == 1280, "and back");
        rr::stop();
        check(!rr::retarget(2.F / 3.F, why) && why == "render scale not active",
              "a stopped override refuses a retarget");
        rr::requested = true;
    }
    { // The game's own aspect switch (game-84): 64:27 in the 2560x1440 window
        // gives a 2560x1080 scene. The override yields instead of faulting,
        // and rebase() resumes it on the new scene.
        rr::discovered.window_width = 2560;
        rr::discovered.window_height = 1440;
        put(config.p + 0x2cda, static_cast<unsigned char>(1));
        put(config.p + 0x2cd0, 2560U);
        put(config.p + 0x2cd4, 1440U);
        put(renderer.p + 0x1f448, 2560U);
        put(renderer.p + 0x1f44c, 1440U);
        put(renderer.p + 0x7b440, 1U);
        put(renderer.p + 0x7b43c, 1U);
        step();
        check(rr::fault == 0 && rr::requested && !rr::scene_changed(), "16:9 runs");
        put(renderer.p + 0x7b440, 2U);
        put(renderer.p + 0x7b43c, 2U);
        put(renderer.p + 0x19c, 1080U);
        step();
        check(rr::fault == 0 && !rr::requested && rr::scene_changed(), "the aspect switch yields instead of faulting");
        step();
        check(rr::fault == 0 && rr::scene_changed() && !rr::input_stamp(view.p, stamp),
              "and stays yielded, stamps closed");
        std::string why;
        check(!rr::retarget(.5F, why), "a yielded override refuses a retarget");
        check(rr::rebase({2560, 1080}, 2560, .5F, why) && rr::target.render == rr::Extent{1280, 540} &&
                  !rr::scene_changed() && rr::requested,
              "rebase on the new scene");
        step();
        step();
        check(rr::input_stamp(view.p, stamp) && stamp.render_width == 1280 && stamp.render_height == 540,
              "stamps follow the new scene");
        put(renderer.p + 0x7b440, 1U);
        put(renderer.p + 0x7b43c, 1U);
        put(renderer.p + 0x19c, 1440U);
        step();
        check(rr::fault == 0 && rr::scene_changed(), "the way back yields too");
        check(rr::rebase({2560, 1440}, 2560, .5F, why), "rebase back");
        step();
        step();
        check(rr::input_stamp(view.p, stamp) && stamp.render_height == 720, "16:9 again");
        check(!rr::rebase({2560, 1440}, 2560, .5F, why) && why == "no scene change pending",
              "rebase needs a pending change");
        // A refused scene that is not the game's fit of the window still faults.
        put(renderer.p + 0x19c, 1000U);
        step();
        check(rr::fault == 4, "another scene size faults");
        put(renderer.p + 0x19c, 1440U);
        rr::fault = 0;
        rr::requested = true;
        step();
        step();
        check(rr::input_stamp(view.p, stamp) && stamp.render_height == 720, "fixture restored");
    }
    check(get<float>(renderer.p + 0x200) == 1.F && get<float>(renderer.p + 0x204) == 1.F &&
              rr::reduced.depth_width == 2560,
          "allocation scale unchanged");
    action = 2;
    step();
    action = 0;
    check(rr::ownership.native == .75F, "native updater new request retained");
    step();
    rr::stop();
    step();
    check(!rr::ownership.owned && !rr::restored && get<float>(renderer.p + 0x1f0) == .75F,
          "stop restores latest native request");
    step();
    check(rr::restored && rr::restoration.applied == .75F && rr::restoration.view_scale == .75F,
          "restoration waits for native application");
    check(!rr::active_ready() && !rr::input_stamp(view.p, stamp), "temporal stamp closes at stop");
    // A separate native request between invocations must not be overwritten by
    // stale restoration. Resume only inside this owned-memory fixture.
    rr::requested = true;
    rr::restored = false;
    step();
    put(renderer.p + 0x1f0, 1.F);
    native_expected = 1.F;
    step();
    check(rr::foreign_requests == 1 && rr::ownership.native == 1.F, "external native request wins");
    action = 1;
    exception_step();
    action = 0;
    check(rr::fault == 7 && rr::pending == 0 && !rr::ownership.owned && !rr::requested &&
              get<float>(renderer.p + 0x1f0) == 1.F,
          "exception restores request and drains callback");
    step();
    step();
    check(rr::restored && rr::restoration.applied == 1.F, "exception recovery lets native apply original scale");
    rr::requested = true;
    rr::restored = false;
    rr::fault = 0;
    action = 3;
    exception_step();
    action = 0;
    check(rr::fault == 7 && rr::pending == 0 && !rr::requested,
          "setter exception drains without holding telemetry lock");
    step();
    check(rr::restored && !rr::ownership.owned && get<float>(renderer.p + 0x1f0) == 1.F,
          "setter exception retains native rendering");
    { // Aspect capture: a pending aspect change (requested 64:27, applied
        // still 16:9) starts one; it is written once it holds 256 visits.
        put(renderer.p + 0x7b440, 2U);
        put(renderer.p + 0x7b43c, 1U);
        step();
        put(renderer.p + 0x7b43c, 2U);
        std::ostringstream early;
        const auto before_full = rr::write_captures(early);
        for (unsigned i = 0; i < 300; ++i) step();
        std::ostringstream c;
        const auto written = rr::write_captures(c);
        check(before_full + written >= 1 && c.str().find("\"aspect\":[2,1]") != std::string::npos &&
                  c.str().find("\"complete\":true") != std::string::npos,
              "an aspect change is captured");
        std::ostringstream again;
        check(rr::write_captures(again) == 0, "each capture is written once");
    }
    std::ostringstream out;
    rr::write(out);
    check(out.str().find("\"dlss_evaluations\":0") != std::string::npos, "diagnostic scope explicit");
    std::cout << "Render-resolution geometry, ownership, native update, ABI, stop and exception checks passed.\n";
}

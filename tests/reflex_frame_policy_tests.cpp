#include "reflex/frame_policy.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using namespace mhw::reflex;
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
FrameStamp start{100, 50, 0, true}, render{101, 51, 1, true};
void complete(FramePolicy& policy, FrameStamp before, FrameStamp after, unsigned thread = 7) {
    require(policy.begin(thread, before).actions == 7, "token/sleep/simulation start");
    require(policy.render_begin(thread, after).actions == 24, "simulation end/render start");
    require(policy.render_end(thread, after).actions == 32, "render end");
    require(policy.present_begin(thread, after, false).actions == 64, "present start");
    require(policy.present_end(thread, after, false).actions == 128, "present end");
    require(policy.end(thread).accepted && !policy.disabled(), "complete frame");
}
void tests() {
    FramePolicy borrowing;
    require(!borrowing.input_token_available(7), "no input token outside a frame");
    borrowing.begin(7, start);
    require(borrowing.input_token_available(7) && !borrowing.input_token_available(8), "token belongs to frame owner");
    borrowing.render_begin(7, render); borrowing.render_end(7, render);
    require(borrowing.input_token_available(7), "matching inputs may be tagged before Present");
    borrowing.present_begin(7, render, false);
    require(!borrowing.input_token_available(7), "no input mutation during Present");
    borrowing.present_end(7, render, false); borrowing.end(7);
    require(!borrowing.input_token_available(7), "no stale frame token after end");
    borrowing.begin(7, render); borrowing.abort();
    require(!borrowing.input_token_available(7), "no input token after feature fault");

    FramePolicy healthy;
    complete(healthy, start, render); require(healthy.id() == 1, "first owned ID");
    complete(healthy, render, {102, 51, 0, true});
    require(healthy.id() == 2 && healthy.completed() == 2, "paused temporal counter does not reuse ID");

    FramePolicy test_present;
    require(test_present.present_begin(99, {}, true).actions == 0, "test before engine frame");
    test_present.begin(7, start); test_present.render_begin(7, render); test_present.render_end(7, render);
    require(test_present.present_begin(99, {}, true).fault == FrameFault::none, "test leaves interval intact");
    test_present.present_begin(7, render, false);
    require(test_present.present_end(99, {}, true).actions == 0, "nested test ignored");
    test_present.present_end(7, render, false); require(test_present.end(7).accepted, "real present after test");

    for (unsigned mode = 0; mode < 5; ++mode) {
        FramePolicy bad; bad.begin(7, start);
        if (mode == 0) require(bad.render_end(7, render).fault == FrameFault::order, "missing render start");
        if (mode == 1) require(bad.render_begin(8, render).fault == FrameFault::thread, "foreign thread");
        if (mode == 2) require(bad.render_begin(7, {101,51,1,false}).fault == FrameFault::stamp, "unstable stamp");
        if (mode == 3) require(bad.render_begin(7, {102,51,1,true}).fault == FrameFault::counter, "wrong epoch");
        if (mode == 4) require(bad.render_begin(7, {101,51,0,true}).fault == FrameFault::counter, "wrong bank");
        require(bad.disabled() && bad.begin(7, render).actions == 0 && bad.completed() == 0, "fail closed");
    }
    FramePolicy incomplete; incomplete.begin(7, start);
    require(incomplete.end(7).fault == FrameFault::order, "engine end is not simulation end");

    // The game's exit: once its window is destroyed, present12 skips Present,
    // so the engine frame ends after render with no Present (game-70). The
    // integration asks presented() first and closes instead of ending.
    FramePolicy presenting;
    require(!presenting.presented(), "no frame yet");
    presenting.begin(7, start); presenting.render_begin(7, render); presenting.render_end(7, render);
    require(!presenting.presented(), "rendered, Present skipped");
    presenting.present_begin(7, render, false);
    require(!presenting.presented(), "inside Present");
    presenting.present_end(7, render, false);
    require(presenting.presented(), "after Present");
    presenting.end(7);
    require(!presenting.presented() && presenting.completed() == 1, "ended");
    presenting.begin(7, render); presenting.abort(FrameFault::none);
    require(!presenting.presented() && presenting.disabled(), "closed without closing markers");
    FramePolicy changed; changed.begin(7, start); changed.render_begin(7, render);
    require(changed.render_end(7, {101,52,1,true}).fault == FrameFault::counter, "mid-frame stamp change");
    FramePolicy repeat; complete(repeat, start, render);
    require(repeat.present_begin(7, render, false).fault == FrameFault::order, "duplicate real present");
    FramePolicy foreign; complete(foreign, start, render);
    require(foreign.begin(8, render).fault == FrameFault::thread, "engine owner remains fixed");
    FramePolicy exhausted((std::numeric_limits<std::uint32_t>::max)());
    require(exhausted.begin(7, start).fault == FrameFault::exhausted, "do not wrap frame IDs");
    FramePolicy stop; stop.begin(7, start); stop.abort(FrameFault::none);
    require(stop.present_end(7, render, false).actions == 0 && stop.disabled(), "stop creates no synthetic markers");
}
// Optional ignored capture replay: each row is a qualified complete interval's
// initial/final counters. This verifies policy compatibility, not semantic hook
// placement, SDK execution, latency or the existence of rendering-end hooks.
void replay(const char* path) {
    std::ifstream in(path); require(static_cast<bool>(in), "capture replay input");
    FramePolicy policy; std::uint64_t count{};
    std::string row;
    while (std::getline(in, row)) {
        FrameStamp a{}, b{}; unsigned thread{};
        std::istringstream fields(row);
        require(static_cast<bool>(fields >> thread >> a.epoch >> a.temporal >> a.bank >>
                                  b.epoch >> b.temporal >> b.bank), "complete replay row");
        fields >> std::ws;
        require(fields.eof(), "no trailing replay fields");
        a.valid = b.valid = true; complete(policy,a,b,thread); ++count;
    }
    require(in.eof() && count > 0 && policy.completed() == count, "complete capture replay");
    std::cout << "Qualified captured intervals accepted: " << count << '\n';
}
}
int main(int argc, char** argv) {
    try { tests(); if (argc == 2) replay(argv[1]); require(argc <= 2,"arguments"); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}

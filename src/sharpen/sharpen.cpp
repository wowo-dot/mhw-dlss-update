#include "sharpen.hpp"
#include "sharpen_pass.hpp"
#include "../common/json.hpp"
#include "../temporal_present/bindings.hpp"
#include "../temporal_present/observer.hpp"
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <ostream>

namespace mhw::sharpen {
namespace {
std::atomic<unsigned> wanted{};
// The pass and its counters: the recording threads and the worker (prepare,
// reset, the log), under the lock.
std::mutex mutex;
std::unique_ptr<SharpenPass> pass;
std::string refusal, error;
std::uint64_t recorded{}, recorded_hdr{};
// Frames not sharpened: no pass, the HDR state not read, unknown bindings, a
// scene NIS refuses, a scratch image of another size, no descriptor slot, an
// implausible white level, a failure while recording.
std::uint64_t unprepared{}, unknown{}, no_restore{}, layout{}, scratch{}, cache_full{}, white{}, failed{};
// Recordings on a list that had set no pipeline state (its mirror marked unset
// again), and those where that failed (the route then refuses the frame).
std::uint64_t fresh_lists{}, unset_failed{};
float last_white{};
} // namespace

void percent(unsigned p) noexcept {
    wanted.store(p > max_percent ? max_percent : p);
}
unsigned percent() noexcept {
    return wanted.load();
}

bool prepare(ID3D12Device *device, std::string &why) noexcept {
    try {
        // Once per process: a pass from an earlier install may still have
        // work in flight (it is reset only at a drained rebuild).
        {
            std::lock_guard lock(mutex);
            if (pass) return true;
        }
        auto fresh = std::make_unique<SharpenPass>(device);
        std::lock_guard lock(mutex);
        pass = std::move(fresh);
        refusal.clear();
        return true;
    } catch (const std::exception &e) {
        why = e.what();
    } catch (...) {
        why = "sharpen pass";
    }
    try {
        std::lock_guard lock(mutex);
        refusal = why;
    } catch (...) {}
    return false;
}

void reset() noexcept {
    try {
        std::lock_guard lock(mutex);
        if (pass) pass->reset();
    } catch (...) {}
}

void record(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
            D3D12_RESOURCE_STATES scene_state, Hdr hdr) noexcept {
    const auto p = wanted.load();
    if (!sharpening(p) || !list || !scene) return;
    try {
        std::lock_guard lock(mutex);
        if (!pass) {
            ++unprepared;
            return;
        }
        const auto desc = scene->GetDesc();
        Constants constants;
        if (!hdr.known) {
            ++unknown;
            return;
        }
        if (hdr.on && !white_plausible(hdr.white)) {
            ++white;
            last_white = hdr.white;
            return;
        }
        if (desc.Width > UINT32_MAX ||
            !mhw::sharpen::constants(p, static_cast<unsigned>(desc.Width), desc.Height, hdr.on, hdr.white, constants)) {
            ++layout;
            return;
        }
        // A worker's final-scene list may have set no root signature or
        // pipeline state yet (dev.11: about 1 % of frames, in bursts): what it
        // never set stays unset (RestoreBindings' unset_optional).
        std::unique_ptr<temporal_present::RestoreBindings> restore;
        try {
            restore = std::make_unique<temporal_present::RestoreBindings>(list, roots, true, true);
        } catch (...) {
            ++no_restore;
            return;
        }
        const auto restore_all = [&] {
            restore->apply();
            if (!roots.pipeline && !temporal_present::observer::unset_injected_pipeline(list, roots)) ++unset_failed;
        };
        SharpenPass::Result result{};
        try {
            result = pass->record(list, scene, scene_state, constants, hdr.on);
        } catch (const std::exception &e) {
            error = e.what();
            ++failed;
            restore_all();
            return;
        }
        restore_all();
        switch (result) {
        case SharpenPass::Result::recorded:
            ++(hdr.on ? recorded_hdr : recorded);
            if (hdr.on) last_white = hdr.white;
            if (!roots.pipeline) ++fresh_lists;
            break;
        case SharpenPass::Result::layout: ++layout; break;
        case SharpenPass::Result::scratch: ++scratch; break;
        case SharpenPass::Result::cache_full: ++cache_full; break;
        }
    } catch (...) {
        ++failed;
    }
}

void write(std::ostream &out) {
    std::lock_guard lock(mutex);
    out << "{\"percent\":" << wanted.load() << ",\"prepared\":" << (pass != nullptr)
        << ",\"refusal\":" << quoted(refusal) << ",\"recorded\":" << recorded << ",\"recorded_hdr\":" << recorded_hdr
        << ",\"white\":" << last_white << ",\"fresh\":" << fresh_lists << ",\"unset_failed\":" << unset_failed
        << ",\"skipped\":{\"unprepared\":" << unprepared << ",\"unknown\":" << unknown
        << ",\"no_restore\":" << no_restore << ",\"layout\":" << layout << ",\"scratch\":" << scratch
        << ",\"cache_full\":" << cache_full << ",\"white\":" << white << ",\"failed\":" << failed
        << "},\"error\":" << quoted(error) << '}';
}

} // namespace mhw::sharpen

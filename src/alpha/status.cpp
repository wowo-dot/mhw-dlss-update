#include "status.hpp"
#include <mutex>

namespace mhw::alpha::status {
namespace {
std::mutex mutex;
View state;

template <class F> void change(F &&f) noexcept {
    try {
        std::lock_guard lock(mutex);
        f(state);
    } catch (...) {
    }
}
}  // namespace

void loader(Loader l, std::string_view reason) noexcept {
    change([&](View &v) { v.loader = l; v.loader_reason = reason; });
}
void dlss(Stage s, std::string_view reason) noexcept {
    change([&](View &v) { v.dlss = s; v.dlss_reason = reason; });
}
void reflex(Stage s, std::string_view reason) noexcept {
    change([&](View &v) { v.reflex = s; v.reflex_reason = reason; });
}
void generation(Generation g, std::string_view reason) noexcept {
    change([&](View &v) { v.generation = g; v.generation_reason = reason; });
}
void dlss_sizes(unsigned render_width, unsigned render_height, unsigned scene_width, unsigned scene_height) noexcept {
    change([&](View &v) {
        v.render_width = render_width;
        v.render_height = render_height;
        v.scene_width = scene_width;
        v.scene_height = scene_height;
    });
}
void worker_failed(std::string_view reason) noexcept {
    change([&](View &v) {
        if (v.dlss == Stage::starting) { v.dlss = Stage::off; v.dlss_reason = reason; }
        if (v.reflex == Stage::starting) { v.reflex = Stage::off; v.reflex_reason = reason; }
        if (v.generation == Generation::starting || v.generation == Generation::armed) {
            v.generation = Generation::stopped;
            v.generation_reason = reason;
        }
    });
}
View snapshot() {
    std::lock_guard lock(mutex);
    return state;
}
}  // namespace mhw::alpha::status

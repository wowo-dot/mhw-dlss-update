// The depth-lease rule of the continuous runtime (depth_lease.hpp), on plain
// structures shaped like its requests and depth copies.
#include "../src/continuous_dlaa/depth_lease.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool value, const char *what) {
    if (!value) {
        std::cerr << what << '\n';
        std::exit(1);
    }
}
struct Owner {
    std::uintptr_t object{}, resource{};
    bool operator==(const Owner &) const = default;
};
struct Stamp {
    std::uint64_t frame{}, epoch{};
    std::uintptr_t view{}, container{};
    unsigned bank{};
    bool async{};
};
struct Source {
    std::uint64_t frame{}, epoch{};
    std::uintptr_t view{};
    unsigned bank{};
    bool async{};
};
struct SceneDepth {
    std::uintptr_t container{};
    Owner texture;
};
struct Copy {
    Stamp stamp;
    Owner destination;
    std::uint64_t sequence{};
};
// The scaled build's request records the copy it chose; the unscaled one does not.
struct ScaledRequest {
    Source source;
    SceneDepth depth;
    std::uint64_t depth_sequence{};
    bool live{true}, forwarding{};
};
struct Request {
    Source source;
    SceneDepth depth;
    bool live{true}, forwarding{};
};

constexpr Stamp stamp{4182, 4182, 0x1000, 0x2000, 1, false};
constexpr Owner depth_texture{0x3000, 0x4000};
template <class R> R request(std::uint64_t chosen = 0) {
    R r;
    r.source = {stamp.frame, stamp.epoch, stamp.view, stamp.bank, stamp.async};
    r.depth = {stamp.container, depth_texture};
    if constexpr (requires { r.depth_sequence; }) r.depth_sequence = chosen;
    return r;
}
} // namespace
using mhw::continuous_dlaa::depth_copy_belongs;

int main() {
    // Title and loading screens (game-69/70): three copies a frame with the
    // same stamp and destination; the request chose the newest (9507).
    const Copy title[]{{stamp, depth_texture, 9505}, {stamp, depth_texture, 9506}, {stamp, depth_texture, 9507}};
    const auto chosen = request<ScaledRequest>(9507);
    check(!depth_copy_belongs(chosen, title[0]) && !depth_copy_belongs(chosen, title[1]),
          "earlier identical copies are not the request's");
    check(depth_copy_belongs(chosen, title[2]), "the request's own copy is leased");
    // Gameplay: one copy with the request's stamp and destination.
    check(depth_copy_belongs(request<ScaledRequest>(77), Copy{stamp, depth_texture, 77}), "gameplay copy");
    check(!depth_copy_belongs(request<ScaledRequest>(0), Copy{stamp, depth_texture, 77}),
          "a request without a chosen copy leases nothing");
    // Every stamp field and the destination still bind the copy to the request.
    const auto changed = [&](auto edit) {
        Copy c{stamp, depth_texture, 9507};
        edit(c);
        return depth_copy_belongs(chosen, c);
    };
    check(!changed([](Copy &c) { ++c.stamp.frame; }), "frame");
    check(!changed([](Copy &c) { ++c.stamp.epoch; }), "epoch");
    check(!changed([](Copy &c) { ++c.stamp.view; }), "view");
    check(!changed([](Copy &c) { c.stamp.bank ^= 1; }), "bank");
    check(!changed([](Copy &c) { c.stamp.async = !c.stamp.async; }), "async mode");
    check(!changed([](Copy &c) { ++c.stamp.container; }), "container");
    check(!changed([](Copy &c) { ++c.destination.resource; }), "destination texture");
    auto gone = chosen;
    gone.live = false;
    check(!depth_copy_belongs(gone, title[2]), "retired request");
    auto forwarding = chosen;
    forwarding.forwarding = true;
    check(!depth_copy_belongs(forwarding, title[2]), "forwarded request");
    // The unscaled build's request has no chosen copy: the stamp alone decides
    // (its single plain copy per frame).
    const auto plain = request<Request>();
    check(depth_copy_belongs(plain, title[0]) && depth_copy_belongs(plain, title[2]),
          "unscaled: stamp and destination");
    std::cout << "depth lease: the request's own copy among identical ones; stamp, destination and state still bind\n";
    return 0;
}

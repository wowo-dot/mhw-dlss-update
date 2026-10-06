#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <iosfwd>

namespace mhw::aa_control {
inline constexpr unsigned post_queue_limit=16;
struct PostNode {
    std::uintptr_t descriptor{},unit{},vtable{},input{},output{};
    unsigned phase{},order{};
    std::array<unsigned char,3> flags{};
    bool operator==(const PostNode &) const=default;
};
struct PostQueue {
    std::uintptr_t manager{},context{},view{},renderer{};
    std::uint64_t frame{},epoch{};
    unsigned bank{},count{},packet_count{};
    unsigned char collecting{},enabled{},taa{},fxaa{};
    std::array<unsigned,4> rectangle{};
    std::array<std::uintptr_t,2> scratch{},attachments{};
    std::array<PostNode,post_queue_limit> nodes{};
    bool operator==(const PostQueue &) const=default;
};
// CPU observation only. Does not enqueue, select AA, call native functions,
// dereference COM objects, or establish GPU contents/states/lifetime.
bool read_post_queue(const temporal_bindings::Reader &,std::uintptr_t image,
                     std::uintptr_t manager,std::uintptr_t context,std::uintptr_t view,PostQueue &);
bool same_post_frame(const PostQueue &,const PostQueue &) noexcept;
// Serialize only an already captured bounded snapshot; no process reads.
void write_post_queue(std::ostream &,const PostQueue &);
// Preparation contract for a future owned pass. Never use a partial plan to
// edit a live queue. Backend binding and lifetime checks remain required.
enum class QueueAdmission {ready,wrong_phase,full,native_aa_present,duplicate,invalid};
QueueAdmission qualify_owned_slot(const PostQueue &,std::uintptr_t image,std::uintptr_t owned_descriptor) noexcept;
}

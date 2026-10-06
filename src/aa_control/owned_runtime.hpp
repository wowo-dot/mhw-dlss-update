#pragma once
#include "owned_pass.hpp"
#include <iosfwd>
namespace mhw::aa_control::owned {
using Publish=bool(*)(unsigned,const PostQueue &,const temporal_bindings::Owner &) noexcept;
// Optional caller-owned diagnostics. Filled on the scheduler thread and copied
// before publishing a fault; log writers never inspect a live Slot.
struct Admission {
    const char *step{"none"};
    unsigned current_slot{},attached_count{},attach_count{},attach_collecting{};
    bool previous_inserted{},previous_removed{},previous_complete{};
    std::uintptr_t descriptor{};
    std::array<std::uintptr_t,post_queue_limit> attach_entries{};
    temporal_bindings::Owner first_owner,second_owner;
    PostQueue first_queue,second_queue;
};
struct ReturnSnapshot {
    OwnedPlan plan;
    PostQueue callback,after,restored;
    unsigned reason{},callbacks{};
    bool inserted{},removed{},returned{},complete{},callback_valid{},neutral{},published{};
};
void configure(std::uintptr_t image,std::uintptr_t device,Publish=nullptr) noexcept;
bool insert(unsigned,const PostQueue &,Admission * = nullptr) noexcept;
// Always attempt scoped removal, even after a native exception. Never free the
// descriptor/unit while the process lives. No private GPU work exists here.
bool remove(unsigned,bool returned) noexcept;
void write(std::ostream &,unsigned,void (*)(std::ostream &,const PostQueue &));
bool detached() noexcept;
// Reuse only after synchronous scheduler return and exact queue restoration.
// Deferred marker lifetime belongs to the publisher, not this descriptor.
bool recycle(unsigned,Admission * = nullptr) noexcept;
void write_admission(std::ostream &,const Admission &,void (*)(std::ostream &,const PostQueue &));
// Copy immediately after remove, on the serialized scheduler thread.
ReturnSnapshot returned_snapshot(unsigned) noexcept;
void write_return(std::ostream &,const ReturnSnapshot &);
#ifdef MHW_AA_OWNED_FIXTURE
bool fixture_attach(const OwnedPlan &,Admission * = nullptr) noexcept;
bool fixture_detach() noexcept;
#endif
}

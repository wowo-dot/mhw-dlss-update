#pragma once
#include "post_queue.hpp"

namespace mhw::aa_control {
// Native post descriptor layout. +21 is the engine's in-place/no-input marker.
// An interior marker does not advance the ping-pong target. No game layer object
// is fabricated: only the reviewed phase's vtable+60 callback may use our unit.
struct OwnedDescriptor {
    unsigned phase{15},order{2};
    std::uintptr_t unit{},input{},output{};
    unsigned char skip{},in_place{1},last_copy{},padding[5]{};
};
static_assert(sizeof(OwnedDescriptor)==0x28 && offsetof(OwnedDescriptor,unit)==8 &&
              offsetof(OwnedDescriptor,input)==0x10 && offsetof(OwnedDescriptor,in_place)==0x21);
struct OwnedPlan {
    std::uintptr_t descriptor{},unit{},vtable{},color{};
    PostQueue before,assigned;
};
// Admit the base AA-Off queue and its observed optional haze filter. Collection
// endpoints may be stale or zero; assigned predicts the hash-gated scheduler's
// complete owner rewrite. Insertion must not alter those native endpoints.
bool plan_owned_pass(const PostQueue &,std::uintptr_t image,std::uintptr_t descriptor,
                     std::uintptr_t unit,std::uintptr_t vtable,OwnedPlan &) noexcept;
bool owned_callback_matches(const OwnedPlan &,const PostQueue &) noexcept;
bool owned_return_matches(const OwnedPlan &,const PostQueue &) noexcept;
bool owned_restored_matches(const OwnedPlan &,const PostQueue &) noexcept;
}

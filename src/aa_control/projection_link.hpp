#pragma once
#include "post_queue.hpp"

namespace mhw::aa_control {
// Immutable producer identity, not the renderer's later consume-time counter.
// The caller reads async_mode during this same scheduler invocation. The jitter
// recorder compares every field to its completed, scoped projection receipt.
inline bool projection_link(const PostQueue &q,unsigned async_mode,temporal_bindings::Snapshot &out) noexcept {
    if(!q.renderer||q.renderer>UINTPTR_MAX-0x710||q.context!=q.renderer+0x710||!q.view||
       !q.frame||!q.epoch||q.bank>1||async_mode>1||q.taa||q.fxaa)return false;
    out={};out.context=q.context;out.view=q.view;out.frame=q.frame;out.epoch=q.epoch;
    out.bank=q.bank;out.async=async_mode;return true;
}
}

#pragma once
#include "../astera/astera_hooks.hpp"
#include <vector>
#include "../temporal_bindings/inputs.hpp"
#include <d3d12.h>
#include <iosfwd>
#include <filesystem>
namespace mhw::temporal_present { struct Bindings; }
namespace mhw::scaled_temporal::stream_route {
// Optional final-scene observer (frame generation build). Called on the
// recording thread before any UI draw, right after the frame's final step:
// - a replaced final-screen draw whose native state was verified restored; the
//   final scene target is the only RTV and in RENDER_TARGET (scaled presets);
// - the native final copy at render scale 1.0 (DLAA); the target is in
//   COPY_DEST (dlaa-identity-path.md).
// The observer may record work on the list but must leave state equal to the
// given bindings; that is verified after it.
using FinalSceneObserver = void (*)(ID3D12GraphicsCommandList *, const temporal_present::Bindings &, ID3D12Resource *scene,
                                    std::uint64_t frame, std::uint64_t epoch, D3D12_RESOURCE_STATES scene_state) noexcept;
void observe_final_scene(FinalSceneObserver) noexcept;
// Hook creation is queued; the owning continuous runtime applies it only after
// every participant is prepared. This module never owns Reset/Execute/barriers.
// width/height: the output (scene) extent; render_width/render_height: the
// game's render extent inside it (render_resolution::plan).
bool prepare(const temporal_bindings::Config &,unsigned width,unsigned height,unsigned render_width,unsigned render_height,std::string &);
void begin() noexcept;
// The original CopyTextureRegion, bypassing this module's copy observer. For
// owner-recorded copies between private and native textures; false before the
// hook exists.
bool copy_region(ID3D12GraphicsCommandList *,const D3D12_TEXTURE_COPY_LOCATION *destination,UINT x,UINT y,UINT z,
                 const D3D12_TEXTURE_COPY_LOCATION *source,const D3D12_BOX *) noexcept;
void publish(const temporal_bindings::Snapshot &,const temporal_bindings::Packet &) noexcept;
// Called after the admitted native postqueue returns successfully, before its
// owning camera frame ends. Individual filter returns do not complete the queue.
void post_queue_complete(std::uintptr_t manager,std::uintptr_t context,std::uintptr_t view,std::uintptr_t taa) noexcept;
// full_post (full_post.hpp), from the activation hooks on the render thread:
// the post scheduler's entry and exit, and the return of TAA's callback.
void full_post_enter(std::uintptr_t context,bool admitted) noexcept;
void full_post_taa(std::uintptr_t context) noexcept;
void full_post_exit(std::uintptr_t context) noexcept;
// From the continuous runtime's TAA consumer, once it proved the scene depth's
// and GBuffer4's states there and copied both for DLSS: the states full_post's
// input upscale starts from (full_post_slot.inl).
void full_post_states(const temporal_bindings::Snapshot &,unsigned depth_state,std::uintptr_t depth,
                      unsigned gbuffer4_state,std::uintptr_t gbuffer4) noexcept;
bool ready(ID3D12GraphicsCommandList *,std::uint64_t generation,const temporal_bindings::Snapshot &,std::uintptr_t recording) noexcept;
bool full_contract(const temporal_bindings::Snapshot &) noexcept;
bool owns_packet(std::uintptr_t) noexcept;
bool observe_resolve(std::uintptr_t,ID3D12GraphicsCommandList *,std::uint64_t) noexcept;
void finish_resolve(std::uintptr_t,bool returned) noexcept;
void resolved(bool,bool reconstructed=true) noexcept;
bool submission_needed(std::span<const std::array<std::uint64_t,2>>) noexcept;
bool validate_submission(std::uint64_t queue,std::span<const std::array<std::uint64_t,2>>,std::uint64_t signal) noexcept;
void reject_submission(std::span<const std::array<std::uint64_t,2>>) noexcept;
bool submitted_batch(std::uint64_t queue,std::span<const std::array<std::uint64_t,2>>,std::uint64_t signal) noexcept;
void observe_submission(std::uint64_t queue,std::uint64_t tick,std::span<const std::array<std::uint64_t,2>>) noexcept;
void barriers(ID3D12GraphicsCommandList *,unsigned,const D3D12_RESOURCE_BARRIER *) noexcept;
// True records both selector-controlled alternatives; suppress only that
// original API call. Unqualified indexed draws retain the native path.
bool indexed_draw(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,int,unsigned,std::uintptr_t) noexcept;
bool reset(ID3D12GraphicsCommandList *,std::uint64_t) noexcept;
void collect(std::uint64_t) noexcept;
bool drained() noexcept;
bool callbacks_idle() noexcept;
// Fault shutdown only: seal undecided choices and retain unproven pins.
// Requires stopped admission and no route callback; this is not retirement.
bool abandon() noexcept;
void stop() noexcept;
// Live preset change (continuous_dlaa::change_mode). Stopped, not faulted,
// no route list active, no plan prepared or running DOF, no depth capture
// awaiting submission: nothing of the route can still record.
bool idle() noexcept;
// With idle() true and every route submission complete on the GPU: adopt a
// new render extent, and for a live aspect change a new output (scene)
// extent, drop the effect and silhouette pipelines built for the old ones
// (their geometry is baked in) and the plans not prepared. begin() then
// resumes. False (extents unchanged) if not idle or the extents are invalid.
bool resize(unsigned render_width,unsigned render_height,unsigned width,unsigned height,std::string &reason) noexcept;
// Takes the game hooks out at once; without AL (research builds) also queues
// CopyTextureRegion's MinHook disable for the runtime's apply. disabled(true)
// after that apply marks the route unhooked.
bool unhook() noexcept;
void disabled(bool) noexcept;
// AL: the CopyTextureRegion detour with the storage AL fills; it runs
// while chains_live(true) (set where the runtime applies the stream hooks).
void astera_hooks(std::vector<astera::Hook>&);
void chains_live(bool) noexcept;
bool healthy() noexcept;
void write(std::ostream &);
// Counters only for the alpha's minute log: plan rejections by reason,
// readiness rejections by reason, and the latest rejected plan graph when
// there were new rejections since the previous call.
void write_summary(std::ostream &);
}

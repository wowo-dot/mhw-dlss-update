#pragma once
#include "../temporal_bindings/inputs.hpp"
#include "../frame_generation/presentation_buffers.hpp"
#include <d3d12.h>
#include <iosfwd>
#include <filesystem>
namespace mhw::scaled_temporal::stream_route {
// Hook creation is queued; the owning continuous runtime applies it only after
// every participant is prepared. This module never owns Reset/Execute/barriers.
bool prepare(const temporal_bindings::Config &,unsigned width,unsigned height,std::string &);
void begin() noexcept;
void publish(const temporal_bindings::Snapshot &,const temporal_bindings::Packet &) noexcept;
// Called after the admitted native postqueue returns successfully, before its
// owning camera frame ends. Individual filter returns do not complete the queue.
void post_queue_complete(std::uintptr_t manager,std::uintptr_t context,std::uintptr_t view,std::uintptr_t taa) noexcept;
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
// Existing executor hook: bounded diagnostic windows plus qualified native DOF
// sequence. True from compute_dispatch means both conditional alternatives were
// recorded; the caller must suppress only that original API call. Other compute
// passes preserve their native call. Token 12 is a rendering-only callback scope.
unsigned compute_before(std::uintptr_t executor,std::uintptr_t backend,std::uintptr_t packet) noexcept;
bool compute_dispatch(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,std::uintptr_t caller) noexcept;
void compute_after(unsigned token,bool returned) noexcept;
bool draw(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,unsigned,std::uintptr_t) noexcept;
// True records both selector-controlled alternatives; suppress only that
// original API call. Unqualified indexed draws retain the native path.
bool indexed_draw(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,int,unsigned,std::uintptr_t) noexcept;
bool reset(ID3D12GraphicsCommandList *,std::uint64_t) noexcept;
void collect(std::uint64_t) noexcept;
void save_compute_depth(const std::filesystem::path &prefix) noexcept;
bool drained() noexcept;
bool callbacks_idle() noexcept;
// Opt-in metadata probe. No FG tags, resource ownership or GPU work.
// Buffer snapshots are host GetBuffer labels: identity metadata, never owned.
bool scene_request(unsigned request,const frame_generation::PresentationBuffers &buffers={}) noexcept;
void scene_present(std::uint64_t frame,std::uint64_t epoch,unsigned reflex_id,unsigned thread,std::uintptr_t swap,
    const frame_generation::PresentationBuffers &buffers={}) noexcept;
// Fault shutdown only: seal undecided choices and retain unproven pins.
// Requires stopped admission and no route callback; this is not retirement.
bool abandon() noexcept;
void stop() noexcept;
bool queue_disable() noexcept;
void disabled(bool) noexcept;
bool healthy() noexcept;
void write(std::ostream &);
}

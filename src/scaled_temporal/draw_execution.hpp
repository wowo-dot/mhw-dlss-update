#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <iosfwd>
#include <span>
#include <d3d12.h>
namespace mhw::scaled_temporal::draw_execution {
// Qualify the three already-selected native draws at their execution boundary.
// No command, attachment, shader, pipeline, viewport or vertex mutation.
bool install(const temporal_bindings::Config &,std::string &);
bool active() noexcept;
bool publish(unsigned frame_index,unsigned stage,const temporal_bindings::Snapshot &,
    std::uintptr_t packet,const temporal_bindings::Owner &output,std::span<const float> vertices={}) noexcept;
#ifdef MHW_SCALED_OUTPUT
bool draw_override(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,unsigned,std::uintptr_t) noexcept;
#endif
bool healthy() noexcept;
bool complete() noexcept;
bool settled() noexcept;
void stop() noexcept;
bool finish() noexcept;
bool save(const std::wstring &);
void write(std::ostream &);
void dispatch(projection_binding::Frame *);
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const temporal_bindings::Config &,void *);
unsigned fixture_publications() noexcept;
#endif
}

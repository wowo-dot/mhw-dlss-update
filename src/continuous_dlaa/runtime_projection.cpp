// The camera's jittered projection: sCamera::draw computes each visible
// viewport's projection (sub_14228F3B0, hook 4); the main scene's gets the
// frame's jitter injected and is captured with its matrices for TAA's request.
#include "runtime_state.hpp"

namespace mhw::continuous_dlaa::detail {
namespace {
bool provider(pb::Provider &p) noexcept {
  unsigned char use{};
  p.valid=pb::read(cfg.renderer+0xa5,use) && use<=1 && pb::read(cfg.renderer+0xb4,p.sizing) && !(p.sizing&1) &&
    pb::read(cfg.renderer+0x78,p.backend) && p.backend==cfg.backend && pb::read(p.backend+0x18,p.device) && p.device==cfg.device &&
    pb::read(p.backend+0x42700,p.wrapper) && !p.wrapper;
  p.use=use;return p.valid;
}
}  // namespace
// sCamera::draw (0x228e770) computes the projection of each visible viewport
// it draws (sub_14228F3B0; sCamera::Viewport, 0x1a0 bytes each from camera
// +0x50: +0x18 mpSceneTexture, +0x20 mVisible, +0x25 mDisplay). The main scene
// is the visible viewport of display 0 without a scene texture (drawn into the
// view the frame owns). sMhCamera's camera switch (sub_141AC3840) shows
// viewport 0, 1 or 2: the tent's and the map's cameras use another than 0
// (game-123: only viewport 0 was accepted, so every tent frame lacked its
// projection and TAA's producer refused it).
bool anchor(pb::Source &s,const NativeFrame &f) noexcept {
  auto &a=s.anchor;unsigned char active{},display{},async{};unsigned stamp{},flags{};std::uint64_t zero{};std::uintptr_t scene_texture{};
  if(!pb::read(cfg.image+0x51c4480,a.renderer) || a.renderer!=cfg.renderer || a.renderer>UINTPTR_MAX-0x1f570 ||
     !pb::read(cfg.image+0x51c4368,a.table) || !a.table || a.table>UINTPTR_MAX-0xe740 ||
     !pb::read(cfg.image+0x51c4400,s.camera) || !s.camera || s.camera>UINTPTR_MAX-0x2000 ||
     f.call.gpr[1]<s.camera+0x50 || (f.call.gpr[1]-s.camera-0x50)%0x1a0 || (f.call.gpr[1]-s.camera-0x50)/0x1a0>=8 ||
     f.call.gpr[2]!=a.renderer+0x710)return false;
  a.context=f.call.gpr[2];const auto viewport=f.call.gpr[1];
  if(!pb::read(s.camera,s.camera_type) || (s.camera_type!=cfg.image+0x350d188 && s.camera_type!=cfg.image+0x33f82a8) ||
     !pb::read(viewport+0x20,active) || !active || !pb::read(viewport+0x25,display) || display ||
     !pb::read(viewport+0x18,scene_texture) || scene_texture ||
     !pb::read(s.camera+0x38,zero) || zero || !pb::read(s.camera+0x40,stamp) ||
     !pb::read(a.renderer+0x1f210,a.frame) || !pb::read(a.renderer+0x1f208,a.epoch) || stamp!=static_cast<unsigned>(a.epoch) ||
     !pb::read(a.renderer+0x1f450,a.view) || !a.view || a.view>UINTPTR_MAX-0x60 ||
     !pb::read(a.view+0x58,flags) || (flags&0x840000) ||
     !pb::read(a.renderer+0x1f200,a.bank) || a.bank>1 || !pb::read(a.renderer+0x93,async) || async>1)return false;
  a.async_mode=async;return true;
}
bool projection_before(const NativeFrame &f){
    if(!accepting.load()||reconstruction.failed()||f.call.caller!=cfg.image+0x228e834)return false;
    if(!render_resolution::active_ready())return false;
    if(projection_busy.test_and_set()){fail(2);return false;}
    projection={};pb::Provider provider_state;
    auto &s=projection.source;
    if(!anchor(s,f)||!provider(provider_state)||!pb::read(s.anchor.table+0xe6a0+8*(s.anchor.frame%20),projection.candidate)||!pb::jitter_finite(projection.candidate)){
        projection_busy.clear();return false;}
    ++projection_viewports[(f.call.gpr[1]-s.camera-0x50)/0x1a0];
    if(!reconstruction.jitter()){projection.candidate=0;++zero_jitter_projections;}
    if(!pb::jitter_exchange(s.camera+0x38,0,projection.candidate)){fail(3);projection_busy.clear();return false;}
    ++jitter_count;return true;
}
void projection_after(bool admitted,bool returned,NativeFrame &f){
    if(!admitted)return;
    auto &s=projection.source;
    // Restore regardless of stop, failed validation, or an exception in native code.
    s.projection=bridge_probe::capture_projection_snapshot(cfg.image,s.anchor,bridge_probe::read_bounded_scalar,nullptr);
    const auto &p=s.projection;
    const bool restored=pb::jitter_exchange(s.camera+0x38,projection.candidate,0);
    if(restored)++restorations;else fail(4);
    const auto bits=std::uint64_t(p.jitter_bits[0])|(std::uint64_t(p.jitter_bits[1])<<32);
    bool valid=returned&&(f.call.gpr[0]&255)&&restored&&p.valid&&p.matrix_relation&&p.manager==s.camera&&p.vtable==s.camera_type&&bits==projection.candidate&&
        p.tagged_constants==p.constants&&p.words&&p.words<=pb::max_constants/4;
    if(valid){s.bytes=p.words*4;std::array<std::byte,pb::max_constants> again{};pb::Provider after;
        valid=pb::copy_memory(p.constants,s.data.data(),s.bytes)&&pb::copy_memory(p.constants,again.data(),s.bytes)&&
            std::memcmp(s.data.data(),again.data(),s.bytes)==0&&pb::capture_matrices(cfg.image,s)&&provider(after);}
    if(valid){LARGE_INTEGER tick{},frequency{};QueryPerformanceCounter(&tick);QueryPerformanceFrequency(&frequency);
        auto &h=projection.history;h.frame=s.anchor.frame;h.epoch=s.anchor.epoch;h.camera=s.camera;h.view=s.anchor.view;
        h.microseconds=static_cast<std::uint64_t>((tick.QuadPart/frequency.QuadPart)*1000000+(tick.QuadPart%frequency.QuadPart)*1000000/frequency.QuadPart);
        h.viewport=p.viewport_bits;h.projection=p.unjittered_bits;for(unsigned i=0;i<16;++i)h.view_matrix[i]=std::bit_cast<float>(s.matrices.bits[1][i]);
        projection.valid=true;Lock lock;projections[h.frame%projections.size()]=projection;
    }else fail(5);
    projection_busy.clear();
}
}  // namespace mhw::continuous_dlaa::detail

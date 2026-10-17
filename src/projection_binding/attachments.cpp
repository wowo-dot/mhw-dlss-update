#include "memory.hpp"
#include <windows.h>
#include <d3d12.h>
namespace mhw::projection_binding {
namespace {
bool address(std::uintptr_t p,unsigned n) noexcept {return p && p<=UINTPTR_MAX-n;}
template<class T> bool field(std::uintptr_t p,unsigned off,T &v) noexcept {
  return address(p,off+sizeof(T)) && copy_memory(p+off,&v,sizeof(T));
}
bool view(std::uintptr_t p,bool depth,ViewBinding &v) noexcept {
  v.owner=p;if(!p)return true;
  if(!field(p,0x60,v.backend) || !v.backend || !field(v.backend,0x10,v.handle) || !v.handle ||
     !field(v.backend,0x18,v.resource) || !v.resource)return false;
  if(depth)return field(p,0x74,v.format);
  unsigned char rotation{};
  if(!field(v.backend,0x20,v.format) || !field(v.backend,0x28,rotation))return false;
  v.rotating=rotation;
  // No length for the alternative handle table is established. Preserve the
  // discriminator and fail closed instead of guessing a rotation bound.
  if(rotation) {field(v.backend,0x2c,v.rotation_index);return false;}
  return true;
}
bool native_pipeline(std::uintptr_t image,std::uintptr_t packet,NativePipeline &p,unsigned depth_format) noexcept {
  if(!field(packet,0x20,p.graphics) || !field(packet,0x28,p.render) ||
     !field(p.graphics,0,p.graphics_flags) || !field(p.graphics,8,p.shader_handle) ||
     !field(p.graphics,0x18,p.group) || !field(p.graphics,0x20,p.input_layout))return false;
  if(p.input_layout && (!field(p.input_layout,0x10,p.input_elements) || !field(p.input_layout,0x18,p.input_count) ||
       p.input_count>32 || (p.input_count && !p.input_elements)))return false;
  unsigned char variant{};
  if(!field(p.render,4,variant) || !field(p.render,0x18,p.raster_owner) ||
     !field(p.render,0x10,p.depth_owner) || !field(p.render,8,p.blend_owner))return false;
  p.raster_variant=variant&1;
  p.dsv_format=depth_format==53?55:depth_format==44?45:depth_format==39?40:depth_format;
  if(!depth_format) {
    std::uintptr_t renderer{},owner{};
    if(!field(image,0x51c73d0,renderer) || !field(renderer,0x2480,owner) || !field(owner,0,p.depth_owner))return false;
  }
  if(!field(p.depth_owner,0x60,p.depth_state) || !field(p.blend_owner,0x98,p.blend_state) ||
     !field(p.raster_owner,0x68+8*p.raster_variant,p.raster_state))return false;
  return field(p.depth_state,0x10,p.depth) && field(p.blend_state,0x10,p.blend) && field(p.raster_state,0x10,p.raster);
}
}
bool snapshot_native_pipeline(std::uintptr_t image,std::uintptr_t packet,NativePipeline &p,unsigned depth_format) noexcept {
  p={};return native_pipeline(image,packet,p,depth_format);
}
bool snapshot_attachments(std::uintptr_t image,const DrawSite &d,AttachmentSnapshot &s) noexcept {
  s={};
  if(!field(d.executor,8,s.state) || !field(s.state,0x20,s.command) || s.command!=d.command ||
     !field(d.packet,0x10,s.attachment) || !field(d.packet,0x38,s.incoming_pso) ||
     !field(d.context,0x10,s.active_attachment) || !field(s.attachment,0x48,s.attachment_flags))return false;
  s.color_count=s.attachment_flags&7;
  for(unsigned i=0;i<s.color_count;++i) {
    std::uintptr_t p{};if(!field(s.attachment,8*i,p) || !view(p,false,s.views[i]))return false;
  }
  std::uintptr_t p{};
  if(!field(s.attachment,0x40,p) || !view(p,true,s.views[8]) || !field(s.state,0x98,s.cached) ||
     !field(s.state,0x140,s.cached_depth_resource) || !field(s.state,0xe0,s.viewport) || !field(s.state,0xf0,s.scissor))return false;
  unsigned char enabled{};
  if(!field(s.state,0x100,enabled) || enabled>1)return false;
  s.scissor_enabled=enabled;
  if(!native_pipeline(image,d.packet,s.pipeline,s.views[8].format))return false;
  s.valid=true;return true;
}
bool attachment_inputs_equal(const AttachmentSnapshot &a,const AttachmentSnapshot &b) noexcept {
  return a.valid && b.valid && a.state==b.state && a.command==b.command && a.attachment==b.attachment &&
    a.incoming_pso==b.incoming_pso && a.attachment_flags==b.attachment_flags && a.color_count==b.color_count &&
    a.views==b.views && a.pipeline==b.pipeline;
}
bool attachment_cache_matches(const AttachmentSnapshot &s) noexcept {
  if(!s.valid || s.active_attachment!=s.attachment)return false;
  for(unsigned i=0;i<9;++i)if(s.views[i].handle!=s.cached[i])return false;
  return !s.views[8].owner || s.cached_depth_resource==s.views[8].resource;
}
}

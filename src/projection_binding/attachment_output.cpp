#include "attachments.hpp"
#include <ostream>
namespace mhw::projection_binding {
namespace {
template<class T,std::size_t N>void arr(std::ostream &o,const std::array<T,N> &a){o<<'[';for(unsigned i=0;i<N;++i){if(i)o<<',';o<<a[i];}o<<']';}
void hex(std::ostream &o,const std::byte *p,unsigned n){constexpr char d[]="0123456789abcdef";o<<'"';for(unsigned i=0;i<n;++i){const auto v=std::to_integer<unsigned>(p[i]);o<<d[v>>4]<<d[v&15];}o<<'"';}
void snapshot(std::ostream &o,const AttachmentSnapshot &s) {
  o<<"{\"valid\":"<<s.valid<<",\"state\":"<<s.state<<",\"command\":"<<s.command<<",\"attachment\":"<<s.attachment
   <<",\"active_attachment\":"<<s.active_attachment<<",\"incoming_pso\":"<<s.incoming_pso<<",\"color_count\":"<<s.color_count
   <<",\"flags\":"<<s.attachment_flags<<",\"scissor_enabled\":"<<s.scissor_enabled<<",\"cached_depth_resource\":"<<s.cached_depth_resource<<",\"cached\":";
  arr(o,s.cached);o<<",\"viewport\":";arr(o,s.viewport);o<<",\"scissor\":";arr(o,s.scissor);o<<",\"views\":[";
  for(unsigned i=0;i<9;++i){if(i)o<<',';const auto &v=s.views[i];o<<"{\"owner\":"<<v.owner<<",\"backend\":"<<v.backend
    <<",\"resource\":"<<v.resource<<",\"handle\":"<<v.handle<<",\"format\":"<<v.format<<",\"rotating\":"<<v.rotating<<",\"rotation_index\":"<<v.rotation_index<<'}';}
  const auto &p=s.pipeline;
  o<<"],\"pipeline\":{\"render\":"<<p.render<<",\"graphics\":"<<p.graphics<<",\"shader_handle\":"<<p.shader_handle
   <<",\"input_layout\":"<<p.input_layout<<",\"input_elements\":"<<p.input_elements<<",\"input_count\":"<<p.input_count
   <<",\"graphics_flags\":"<<p.graphics_flags<<",\"group\":"<<p.group<<",\"raster_variant\":"<<p.raster_variant<<",\"dsv_format\":"<<p.dsv_format
   <<",\"depth_owner\":"<<p.depth_owner<<",\"depth_state\":"<<p.depth_state<<",\"blend_owner\":"<<p.blend_owner
   <<",\"blend_state\":"<<p.blend_state<<",\"raster_owner\":"<<p.raster_owner<<",\"raster_state\":"<<p.raster_state<<",\"depth\":";
  arr(o,p.depth);o<<",\"blend\":";arr(o,p.blend);o<<",\"raster\":";arr(o,p.raster);o<<"}}";
}
}
void write_attachments(std::ostream &o,const AttachmentCapture &c) {
  o<<"{\"enabled\":"<<c.enabled<<",\"described\":"<<c.described<<",\"at_draw\":"<<c.at_draw<<",\"stable\":"<<c.stable
   <<",\"resources_released\":"<<c.resources_released<<",\"state_order\":"<<c.state_order<<",\"draw_order\":"<<c.draw_order
   <<",\"barrier_count\":"<<c.barrier_count<<",\"entry\":";snapshot(o,c.entry);o<<",\"ready\":";snapshot(o,c.ready);
  o<<",\"resources\":[";
  for(unsigned i=0;i<c.resource_count;++i) {
    if(i)o<<',';const auto &r=c.resources[i];
    o<<"{\"valid\":"<<r.valid<<",\"resource\":"<<r.resource<<",\"identity\":"<<r.identity<<",\"device\":"<<r.device
     <<",\"dimension\":"<<r.dimension<<",\"format\":"<<r.format<<",\"width\":"<<r.width<<",\"height\":"<<r.height
     <<",\"depth_or_array\":"<<r.depth_or_array<<",\"mips\":"<<r.mips<<",\"samples\":"<<r.samples<<",\"quality\":"<<r.quality
     <<",\"layout\":"<<r.layout<<",\"flags\":"<<r.flags<<",\"alignment\":"<<r.alignment<<'}';
  }
  o<<"],\"resource_checks\":[";
  for(unsigned phase=0;phase<3;++phase) {
    if(phase)o<<',';o<<"{\"phase\":"<<phase<<",\"queries\":[";
    for(unsigned i=0;i<c.resource_check_counts[phase];++i) {
      if(i)o<<',';const auto &q=c.resource_checks[phase][i];
      o<<"{\"checkpoint\":"<<q.checkpoint<<",\"valid\":"<<q.valid<<",\"temporaries_released\":"<<q.temporaries_released
       <<",\"candidate\":"<<q.candidate<<",\"resource\":"<<q.resource<<",\"device\":"<<q.device<<",\"identity\":"<<q.identity
       <<",\"identity_repeat\":"<<q.identity_repeat<<",\"roundtrip\":"<<q.roundtrip<<",\"roundtrip_identity\":"<<q.roundtrip_identity
       <<",\"results\":";arr(o,q.results);o<<'}';
    }
    o<<"]}";
  }
  o<<"]}";
}
void write_output_binding(std::ostream &o,const OutputBinding &b) {
  o<<"{\"count\":"<<b.count<<",\"contiguous\":"<<b.contiguous<<",\"depth_present\":"<<b.depth_present<<",\"depth\":"<<b.depth<<",\"colors\":";
  arr(o,b.colors);o<<'}';
}
void write_barriers(std::ostream &o,const std::array<BarrierRecord,max_barriers> &a,unsigned n) {
  o<<'[';for(unsigned i=0;i<n;++i){if(i)o<<',';const auto &b=a[i];o<<"{\"type\":"<<b.type<<",\"flags\":"<<b.flags
    <<",\"resource\":"<<b.resource<<",\"other\":"<<b.other<<",\"subresource\":"<<b.subresource<<",\"before\":"<<b.before<<",\"after\":"<<b.after<<'}';}o<<']';
}
void write_pipeline_creation(std::ostream &o,const PipelineCreation &p) {
  o<<"{\"seen\":"<<p.seen<<",\"iid_valid\":"<<p.iid_valid<<",\"bytecode_matches\":"<<p.bytecode_matches<<",\"returned\":"<<p.returned
   <<",\"succeeded\":"<<p.succeeded<<",\"stable\":"<<p.stable<<",\"descriptor_address\":"<<p.descriptor_address<<",\"output_address\":"<<p.output_address<<",\"pso\":"<<p.pso<<",\"root\":"<<p.root
   <<",\"result\":"<<p.result<<",\"input_count\":"<<p.input_count<<",\"color_count\":"<<p.color_count<<",\"dsv_format\":"<<p.dsv_format
   <<",\"samples\":"<<p.samples<<",\"quality\":"<<p.quality<<",\"formats\":";arr(o,p.formats);
  o<<",\"sizes\":";arr(o,p.sizes);o<<",\"data\":";arr(o,p.data);
  if(p.seen) {
    o<<",\"descriptor_hex\":";hex(o,p.descriptor.data(),static_cast<unsigned>(p.descriptor.size()));
    o<<",\"input_elements_hex\":";hex(o,p.input_elements.data(),(p.input_count<=32?p.input_count:0)*32);
    o<<",\"semantics\":[";
    for(unsigned i=0;i<p.input_count && i<32;++i){if(i)o<<',';o<<'"';for(const auto ch:p.semantics[i]){if(!ch)break;if(ch=='"'||ch=='\\')o<<'\\';if(ch>=32 && ch<127)o<<ch;else o<<'?';}o<<'"';}
    o<<']';
  }
  o<<'}';
}
}

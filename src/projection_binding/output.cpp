#include "recorder.hpp"
#include <windows.h>
#include <filesystem>
#include <ostream>
#include <stdexcept>
namespace mhw::projection_binding {
namespace {
template<class T,std::size_t N> void array(std::ostream &o,const std::array<T,N> &a) {
  o<<'[';for(unsigned i=0;i<N;++i){if(i)o<<',';o<<a[i];}o<<']';
}
void arena(std::ostream &o,const Arena &a,const Location &p) {
  o<<"{\"backend\":"<<a.backend<<",\"mode\":"<<a.mode<<",\"allocation_index\":"<<a.index<<",\"stride\":"<<a.stride
   <<",\"cpu_resource\":"<<a.cpu_resource<<",\"gpu_resource\":"<<a.gpu_resource<<",\"delta\":"<<a.delta<<",\"cpu\":";
  array(o,a.cpu);o<<",\"gpu\":";array(o,a.gpu);o<<",\"used\":";array(o,a.used);
  o<<",\"location\":{\"valid\":"<<p.valid<<",\"arena\":"<<p.arena<<",\"offset\":"<<p.offset<<",\"aligned_bytes\":"<<p.aligned_bytes<<",\"gpu\":"<<p.gpu<<"}}";
}
void save(const std::wstring &path,const void *data,unsigned bytes) {
  const auto f=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(f==INVALID_HANDLE_VALUE)throw std::runtime_error("blob-create");
  DWORD written{};const bool ok=WriteFile(f,data,bytes,&written,nullptr) && written==bytes && FlushFileBuffers(f);
  CloseHandle(f);if(!ok)throw std::runtime_error("blob-write");
}
void source(std::ostream &o,const Source &s,const char *event) {
  const auto &a=s.anchor;const auto &p=s.projection;
  o<<"{\"event\":\""<<event<<"\",\"valid\":"<<s.valid<<",\"order\":"<<s.order<<",\"tick\":"<<s.tick<<",\"thread\":"<<s.thread
   <<",\"camera\":"<<s.camera<<",\"camera_type\":"<<s.camera_type<<",\"renderer\":"<<a.renderer<<",\"context\":"<<a.context
   <<",\"view\":"<<a.view<<",\"table\":"<<a.table<<",\"frame\":"<<a.frame<<",\"epoch\":"<<a.epoch<<",\"bank\":"<<a.bank<<",\"async_mode\":"<<a.async_mode
   <<",\"bytes\":"<<s.bytes<<",\"data_sha256\":\""<<(s.valid?bytes_sha256(s.data.data(),s.bytes):"")<<"\",\"projection_valid\":"<<p.valid
   <<",\"projection_rejection\":"<<static_cast<unsigned>(p.rejection)<<",\"projection_checkpoint\":"<<p.checkpoint<<",\"matrix_relation\":"<<p.matrix_relation
   <<",\"buffer_id\":"<<p.buffer_id<<",\"projection_id\":"<<p.projection_id<<",\"unjittered_id\":"<<p.unjittered_id<<",\"words\":"<<p.words
   <<",\"constants\":"<<p.constants<<",\"tagged_constants\":"<<p.tagged_constants<<",\"jitter_bits\":";
  array(o,p.jitter_bits);o<<",\"viewport_bits\":";array(o,p.viewport_bits);o<<",\"projection_bits\":";array(o,p.projection_bits);
  o<<",\"unjittered_bits\":";array(o,p.unjittered_bits);
  o<<",\"matrices_valid\":"<<s.matrices.valid<<",\"matrix_ids\":";array(o,s.matrices.ids);
  o<<",\"view_projection_bits\":";array(o,s.matrices.bits[0]);o<<",\"view_bits\":";array(o,s.matrices.bits[1]);
  o<<",\"unjittered_view_projection_bits\":";array(o,s.matrices.bits[2]);
  o<<",\"table20_bits\":";array(o,p.table20_bits);
  o<<",\"arena\":";arena(o,s.arena,s.location);o<<'}';
}
void provider(std::ostream &o,const Provider &p) {
  o<<"{\"valid\":"<<p.valid<<",\"use\":"<<p.use<<",\"sizing\":"<<p.sizing<<",\"backend\":"<<p.backend
   <<",\"device\":"<<p.device<<",\"wrapper\":"<<p.wrapper<<'}';
}
}
void write_capture(std::ostream &o,const Capture &c,const std::wstring &prefix) {
  o<<std::boolalpha;
  if(c.stimulus.enabled) {
    const auto &x=c.stimulus;
    o<<"{\"event\":\"stimulus\",\"count\":"<<x.count<<",\"injections\":"<<x.injections<<",\"restorations\":"<<x.restorations
     <<",\"recovery_deferrals\":"<<x.recovery_deferrals<<",\"draw_waits\":"<<x.draw_waits<<",\"calls\":[";
    for(unsigned i=0;i<x.count;++i) {
      if(i)o<<',';const auto &j=x.calls[i];
      o<<"{\"stage\":"<<i<<",\"candidate\":"<<j.candidate<<",\"injected\":"<<j.injected<<",\"restored\":"<<j.restored
       <<",\"restored_bits\":"<<j.restored_bits<<",\"returned\":"<<j.returned<<",\"complete\":"<<j.complete<<",\"exit_order\":"<<j.exit_order<<",\"before\":";
      provider(o,j.before);o<<",\"after\":";provider(o,j.after);o<<",\"source\":";source(o,j.source,"cpu_sample");o<<'}';
    }
    o<<"]}\n";
  }
  source(o,c.source,"source");o<<'\n';
  const auto &pub=c.publication;
  o<<"{\"event\":\"publication\",\"valid\":"<<pub.valid<<",\"order\":"<<pub.order<<",\"tick\":"<<pub.tick<<",\"thread\":"<<pub.thread
   <<",\"packet\":"<<pub.packet<<",\"context\":"<<pub.context<<",\"pointer\":"<<pub.pointer<<",\"id\":"<<pub.id<<",\"index\":"<<pub.index<<",\"count\":"<<pub.count<<"}\n";
  const auto &con=c.consumption;
  o<<"{\"event\":\"consumption\",\"valid\":"<<con.valid<<",\"order\":"<<con.order<<",\"tick\":"<<con.tick<<",\"thread\":"<<con.thread
   <<",\"packet\":"<<con.packet<<",\"context\":"<<con.context<<",\"executor\":"<<con.executor<<",\"pointer\":"<<con.pointer<<",\"arena\":";
  arena(o,con.arena,con.location);o<<"}\n";
  for(unsigned i=0;i<c.blob_count;++i) {
    const auto &b=c.blobs[i];const auto path=prefix+L"-shader-"+std::to_wstring(i+1)+L".bin";
    save(path,b.bytes.data(),b.size);
    // Only the fixed generated filename is emitted, not a personally identifying path.
    o<<"{\"event\":\"shader\",\"index\":"<<i+1<<",\"size\":"<<b.size<<",\"sha256\":\""<<bytes_sha256(b.bytes.data(),b.size)<<"\",\"dxbc_structural\":true}\n";
  }
  for(unsigned i=0;i<c.draw_count;++i) {
    const auto &d=c.draws[i];
    o<<"{\"event\":\"draw\",\"index\":"<<i+1<<",\"kind\":"<<d.kind<<",\"thread\":"<<d.thread<<",\"order\":"<<d.order<<",\"exit_order\":"<<d.exit_order
     <<",\"tick\":"<<d.tick<<",\"returned\":"<<d.returned<<",\"state_seen\":"<<d.state_seen<<",\"source_unchanged\":"<<d.source_unchanged
     <<",\"root_binding_observed\":"<<d.root_binding_observed<<",\"descriptor_observed\":"<<d.descriptor_observed<<",\"draw_api_observed\":"<<d.draw_api_observed
     <<",\"rejection\":"<<d.rejection<<",\"gpu\":"<<d.gpu<<",\"executor\":"<<d.executor<<",\"context\":"<<d.context<<",\"packet\":"<<d.packet
     <<",\"graphics\":"<<d.graphics<<",\"shader_set\":"<<d.shader_set<<",\"command\":"<<d.command<<",\"pso_cache\":"<<d.pso<<",\"root_cache\":"<<d.root
     <<",\"device\":"<<d.device<<",\"binding_mode\":"<<d.binding_mode<<",\"heap_cpu\":"<<d.heap_cpu<<",\"heap_gpu\":"<<d.heap_gpu<<",\"descriptor_stride\":"<<d.descriptor_stride
     <<",\"blobs\":";array(o,d.blobs);o<<",\"stage_entries\":";array(o,d.stage_entries);
    const auto &selection=d.selection;
    o<<",\"root_selection\":{\"valid\":"<<selection.valid<<",\"stable\":"<<d.root_stable<<",\"index\":"<<selection.index
     <<",\"vs_flags\":"<<selection.vs_flags<<",\"ps_flags\":"<<selection.ps_flags<<",\"ps_count\":"<<selection.ps_count
     <<",\"descriptor_mode\":"<<selection.descriptor_mode<<",\"recipe\":"<<selection.recipe<<",\"recipe_device\":"<<selection.recipe_device<<",\"recipe_flags\":[";
    for(unsigned j=0;j<3;++j){if(j)o<<',';o<<static_cast<unsigned>(selection.recipe_flags[j]);}
    o<<"],\"signatures\":";array(o,selection.signatures);o<<'}';
    if(d.attachments.enabled){o<<",\"attachments\":";write_attachments(o,d.attachments);o<<",\"pipeline_creation\":";write_pipeline_creation(o,d.creation);}
    o<<"}\n";
  }
  for(unsigned i=0;i<c.api_count;++i) {
    const auto &e=c.apis[i];
    o<<"{\"event\":\"api\",\"index\":"<<i+1<<",\"kind\":"<<e.kind<<",\"draw\":"<<e.draw<<",\"thread\":"<<e.thread
     <<",\"order\":"<<e.order<<",\"exit_order\":"<<e.exit_order<<",\"tick\":"<<e.tick<<",\"object\":"<<e.object<<",\"caller\":"<<e.caller
     <<",\"returned\":"<<e.returned<<",\"size\":"<<e.size<<",\"args\":";array(o,e.args);o<<",\"viewport\":";array(o,e.viewport);
    if(e.kind==output_binding){o<<",\"outputs\":";write_output_binding(o,e.outputs);}
    if(e.kind==barriers){o<<",\"barriers\":";write_barriers(o,e.barrier_records,e.barrier_count);}
    if(e.kind==scissor){o<<",\"scissor\":";array(o,e.scissor);}
    o<<"}\n";
  }
}
}

// Included inside depth_routes' private namespace for the scaled diagnostic.
// Observe the existing native writer; never replace its draw or bindings.
using DrawFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,UINT,UINT);
bool draw_snapshot(const Consumer &r,DrawSnapshot &out){
  const auto &q=requests[r.receipt].value;out={};out.nested=q.packet.nested;
  if(!pointer(out.nested,0x48) || !stable(r) ||
     !reader.get(out.nested+0x10,out.bindings) || !reader.get(out.nested+0x40,out.arguments) ||
     !reader.get(r.recording+0x158,out.recorded_packet) || out.recorded_packet!=out.nested ||
     !reader.get(r.recording+0x108,out.pipeline) || !out.pipeline ||
     !pointer(out.bindings[0],0x5c) || !reader.get(out.bindings[0]+0x4c,out.rectangle))return false;
  out.rectangle_owner=out.bindings[0];
  for(unsigned i=0;i<4;++i)if(out.rectangle[i]!=q.rectangle[i])return false;
  return out.arguments[0]>0 && out.arguments[0]<=6 && !q.rectangle[0] && !q.rectangle[1] &&
    !q.rectangle[4] && !q.rectangle[5] && q.rectangle[2]<=r.destination.desc.Width && q.rectangle[3]<=r.destination.desc.Height &&
    (q.rectangle[2]<r.destination.desc.Width || q.rectangle[3]<r.destination.desc.Height) &&
    r.source.desc.Width==r.destination.desc.Width && r.source.desc.Height==r.destination.desc.Height &&
    r.source.desc.Format==DXGI_FORMAT_R32_TYPELESS && r.source.desc.Flags==D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL &&
    r.destination.desc.Format==DXGI_FORMAT_R32_FLOAT && r.destination.desc.Flags==D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
}
bool draw_before(ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,unsigned instance,std::uintptr_t caller){
  if(current==UINT_MAX || !healthy())return false;
  auto &r=consumers[current];const auto &q=requests[r.receipt].value;
  r.draw=true;++r.native_calls;r.copy=++order;r.caller=caller;r.draw_arguments={n,instances,first,instance};
  const bool ok=r.native_calls==1 && caller==cfg.image+0x259d72e && reinterpret_cast<std::uintptr_t>(list)==r.command &&
    draw_snapshot(r,r.draw_before) && n==r.draw_before.arguments[0] && first==r.draw_before.arguments[1] && instances==1 && !instance &&
    draw_states(std::span(r.barriers.data(),r.barrier_count),q.source.resource,q.destination.resource);
  if(!ok){r.valid=false;fail(17);}return true;
}
void draw_after(bool observed,bool returned){
  if(!observed)return;auto &r=consumers[current];r.copy_exit=++order;r.copy_returned=returned;
  if(!returned || !draw_snapshot(r,r.draw_after) || r.draw_before!=r.draw_after){r.valid=false;fail(18);return;}
  readback_after(r);
}
void draw_call(ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,unsigned instance,std::uintptr_t caller){
  const bool observed=draw_before(list,n,instances,first,instance,caller);bool returned=false;
  __try{
#ifdef MHW_SCALED_OUTPUT
    if(!scaled_temporal::draw_execution::draw_override(list,n,instances,first,instance,caller))
#endif
    reinterpret_cast<DrawFn>(originals[3])(list,n,instances,first,instance);returned=true;}
  __finally{draw_after(observed,returned);}
}
void WINAPI draw_hook(ID3D12GraphicsCommandList *list,UINT n,UINT instances,UINT first,UINT instance){
  draw_call(list,n,instances,first,instance,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
}
void write_draw(std::ostream &out,const Consumer &r){
  const auto &d=r.draw_before;
  out<<",\"operation\":\""<<(r.draw?"draw":"copy")<<"\",\"draw\":{\"stable\":"<<(r.draw&&d==r.draw_after)
     <<",\"nested\":"<<d.nested<<",\"recorded_packet\":"<<d.recorded_packet<<",\"pipeline\":"<<d.pipeline
     <<",\"rectangle_owner\":"<<d.rectangle_owner<<",\"bindings\":[";
  for(unsigned i=0;i<4;++i){if(i)out<<',';out<<d.bindings[i];}out<<"],\"rectangle\":[";
  for(unsigned i=0;i<4;++i){if(i)out<<',';out<<d.rectangle[i];}out<<"],\"packet_arguments\":["<<d.arguments[0]<<','<<d.arguments[1]<<"],\"arguments\":[";
  for(unsigned i=0;i<4;++i){if(i)out<<',';out<<r.draw_arguments[i];}out<<"]}";
}

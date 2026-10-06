#include "motion_readback.hpp"
#include "inputs.hpp"
#include "../projection_binding/recorder.hpp"
#include <filesystem>
#include <ostream>
namespace mhw::temporal_bindings::motion {
namespace {
template<class T>void drop(T *&p) noexcept {if(p){p->Release();p=nullptr;}}
bool same_device(ID3D12DeviceChild *p,ID3D12Device *expected) {
  ID3D12Device *d{};const auto hr=p->GetDevice(IID_PPV_ARGS(&d));const bool ok=SUCCEEDED(hr) && d==expected;drop(d);return ok;
}
}
bool Capture::extent(unsigned w,unsigned h) noexcept {
  return w && h && w<=8192 && h<=8192 && std::uint64_t(w)*h*4<=byte_limit;
}
bool Capture::source_matches(const D3D12_RESOURCE_DESC &d) const noexcept {
  return d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width==width_ && d.Height==height_ &&
    d.DepthOrArraySize==1 && d.MipLevels==1 && d.Format==format_ && d.SampleDesc.Count==1 &&
    !d.SampleDesc.Quality && d.Layout==D3D12_TEXTURE_LAYOUT_UNKNOWN && d.Flags==flags_;
}
bool Capture::source_matches(ID3D12Resource *source) const noexcept {
  if(!source || phase_!=Phase::ready)return false;
  __try{return source_matches(source->GetDesc());}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void Capture::release_unissued() noexcept {
  if(attempted_)return;drop(allocator_);drop(list_);drop(source_);drop(buffer_);drop(owned_);drop(device_);retained_=false;
}
bool Capture::prepare(ID3D12Device *device,unsigned w,unsigned h,Kind kind,ID3D12Resource *owned) noexcept {
  if(phase_!=Phase::disabled || !device || !extent(w,h) || (kind!=Kind::motion && kind!=Kind::color && kind!=Kind::depth))return false;
  if(kind==Kind::color){format_=DXGI_FORMAT_R11G11B10_FLOAT;
    flags_=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;state_=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;}
  if(kind==Kind::depth){format_=DXGI_FORMAT_R32_FLOAT;flags_=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;state_=D3D12_RESOURCE_STATE_COPY_DEST;}
  device_=device;device_->AddRef();width_=w;height_=h;
  if(owned) {
    auto d=owned->GetDesc();const bool flags=d.Flags==D3D12_RESOURCE_FLAG_NONE;d.Flags=flags_;
    if(!flags || !source_matches(d) || !same_device(owned,device)){phase_=Phase::failed;release_unissued();return false;}
    owned_=owned;owned_->AddRef();
  }
  D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;
  d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format_;d.SampleDesc.Count=1;d.Flags=flags_;
  device_->GetCopyableFootprints(&d,0,1,0,&footprint_,&rows_,&row_bytes_,&bytes_);
  const auto &f=footprint_.Footprint;
  bool ok=!footprint_.Offset && rows_==h && row_bytes_==std::uint64_t(w)*4 && f.Width==w && f.Height==h && f.Depth==1 &&
    f.Format==format_ && f.RowPitch==((row_bytes_+255)&~255ULL) &&
    bytes_==std::uint64_t(f.RowPitch)*(h-1)+row_bytes_ && bytes_>0 && bytes_<=byte_limit;
  if(ok) {
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
    d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes_;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;
    d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ok=SUCCEEDED(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&buffer_)));
  }
  if(!ok){phase_=Phase::failed;release_unissued();return false;}
  retained_=true;phase_=Phase::ready;return true;
}
bool Capture::commands(ID3D12GraphicsCommandList *list,BarrierFn barrier) noexcept {
  D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=source_;b.Transition.Subresource=UINT_MAX;
  b.Transition.StateBefore=state_;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=source_;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.pResource=buffer_;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint_;
  // A void call may have recorded work even if it faults: quarantine from the
  // first transition attempt. Restore after any returned transition, including
  // a CopyTextureRegion exception; never retry an uncertain transition.
  attempted_=true;
  __try {
    __try {barrier(list,1,&b);transitioned_=true;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);copied_=true;
      if(owned_){dst={};dst.pResource=owned_;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);owned_copied_=true;}}
    __finally {if(transitioned_){b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;b.Transition.StateAfter=state_;
      barrier(list,1,&b);restored_=true;}}
  }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
  return transitioned_ && copied_ && restored_ && (!owned_ || owned_copied_);
}
bool Capture::issue_impl(ID3D12GraphicsCommandList *list,ID3D12Resource *source,BarrierFn barrier,const Site &site) {
  site_=site;std::uintptr_t copy_target{};
  if(!list || !source || !barrier || site.receipt>=capacity || !site.frame || !site.packet || !site.identity || !site.reset ||
    !(site.reset<site.marker && site.marker<site.begin) || site.end ||
    site.command!=reinterpret_cast<std::uintptr_t>(list) || site.resource!=reinterpret_cast<std::uintptr_t>(source) || !site.allocator ||
    !projection_binding::system_method(site.command,16,copy_target) || list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT ||
    source==owned_ || !source_matches(source->GetDesc()) || !same_device(source,device_) || !same_device(list,device_))return false;
  IUnknown *canonical{};if(FAILED(source->QueryInterface(IID_PPV_ARGS(&canonical))))return false;
  const bool same=reinterpret_cast<std::uintptr_t>(canonical)==site.identity;drop(canonical);if(!same)return false;
  if(FAILED(source->QueryInterface(IID_PPV_ARGS(&source_))) || source_!=source ||
     FAILED(list->QueryInterface(IID_PPV_ARGS(&list_))) || list_!=list ||
     FAILED(reinterpret_cast<IUnknown *>(site.allocator)->QueryInterface(IID_PPV_ARGS(&allocator_))) ||
     !same_device(allocator_,device_))return false;
  return commands(list,barrier);
}
bool Capture::issue(ID3D12GraphicsCommandList *list,ID3D12Resource *source,BarrierFn barrier,const Site &site) noexcept {
  if(phase_!=Phase::ready)return false;
  bool ok=false;__try{ok=issue_impl(list,source,barrier,site);}__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  phase_=ok?Phase::recorded:attempted_?Phase::quarantined:Phase::failed;
  if(!ok)release_unissued();return ok;
}
bool Capture::issue_depth_draw(ID3D12GraphicsCommandList *list,ID3D12Resource *source,BarrierFn barrier,const Site &site) noexcept {
  if(phase_!=Phase::ready || format_!=DXGI_FORMAT_R32_FLOAT || flags_!=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ||
     state_!=D3D12_RESOURCE_STATE_COPY_DEST)return false;
  state_=D3D12_RESOURCE_STATE_RENDER_TARGET;return issue(list,source,barrier,site);
}
void Capture::issue_end(std::uint64_t end) noexcept {if(phase_==Phase::recorded && !site_.end && end>site_.begin)site_.end=end;else abort();}
bool Capture::qualify(const Completion &c) noexcept {
  if(phase_!=Phase::recorded || !site_.end || !restored_)return false;
  const auto &s=c.site;
  const bool ok=s.receipt==site_.receipt && s.frame==site_.frame && s.packet==site_.packet && s.command==site_.command &&
    s.allocator==site_.allocator && s.resource==site_.resource && s.identity==site_.identity && s.reset==site_.reset &&
    s.marker==site_.marker && s.begin==site_.begin && s.end==site_.end &&
    s.end<c.forward && c.forward<c.dispatch_exit && c.dispatch_exit<c.close && c.close<c.execute && c.execute<c.execute_exit &&
    c.execute_exit<c.signal && c.signal<c.signal_exit && c.signal_exit<c.completed && c.execute_exit<c.retire && c.retire<c.retire_exit && c.queue && c.fence;
  if(ok){completion_=c;phase_=Phase::qualified;}else phase_=Phase::quarantined;return ok;
}
void Capture::abort() noexcept {
  if(phase_==Phase::saved)return;phase_=attempted_?Phase::quarantined:Phase::failed;release_unissued();
}
bool Capture::save(const std::wstring &path) {
  if(phase_!=Phase::qualified)return false;
  const std::filesystem::path destination(path);
  // Filenames are generated by the worker, never external paths from the game.
  file_=destination.filename().string();
  const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  void *mapped{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes_)};
#ifdef MHW_TEMPORAL_FIXTURE
  if(map_failure_)map_result_=static_cast<unsigned>(E_FAIL);else
#endif
    map_result_=static_cast<unsigned>(buffer_->Map(0,&range,&mapped));
  bool ok=map_result_<0x80000000 && mapped;
  if(ok) {
    mapped_=true;
    for(unsigned row=0;row<height_ && ok;++row){DWORD written{};
      ok=WriteFile(file,static_cast<const char *>(mapped)+std::size_t(row)*footprint_.Footprint.RowPitch,
                   static_cast<DWORD>(row_bytes_),&written,nullptr) && written==row_bytes_;if(ok)pixel_bytes_+=written;}
    D3D12_RANGE empty{};buffer_->Unmap(0,&empty);
  }
  const bool flushed=FlushFileBuffers(file)!=0,closed=CloseHandle(file)!=0;ok=ok && flushed && closed;
  if(ok){hash_=projection_binding::file_sha256(path);ok=hash_.size()==64;}
  phase_=ok?Phase::saved:Phase::quarantined;return ok;
}
void Capture::write(std::ostream &o) const {
  const auto &s=site_;const auto &f=footprint_.Footprint;
  o<<"{\"phase\":"<<static_cast<unsigned>(phase_)<<",\"attempted\":"<<attempted_<<",\"transitioned\":"<<transitioned_<<",\"copied\":"<<copied_
   <<",\"restored\":"<<restored_<<",\"retained\":"<<retained_<<",\"mapped\":"<<mapped_<<",\"map_result\":"<<map_result_
   <<",\"receipt\":"<<s.receipt<<",\"frame\":"<<s.frame<<",\"packet\":"<<s.packet<<",\"command\":"<<s.command<<",\"allocator\":"<<s.allocator
   <<",\"resource\":"<<s.resource<<",\"identity\":"<<s.identity<<",\"reset\":"<<s.reset<<",\"marker\":"<<s.marker<<",\"begin\":"<<s.begin<<",\"end\":"<<s.end
   <<",\"width\":"<<width_<<",\"height\":"<<height_<<",\"format\":"<<format_<<",\"source_flags\":"<<flags_<<",\"subresource\":0,\"state_before\":"<<state_<<",\"copy_state\":2048,\"state_after\":"<<state_
   <<",\"readback_buffer\":"<<reinterpret_cast<std::uintptr_t>(buffer_)
   <<",\"footprint\":["<<footprint_.Offset<<','<<f.Format<<','<<f.Width<<','<<f.Height<<','<<f.Depth<<','<<f.RowPitch<<"],\"allocation_bytes\":"<<bytes_
   <<",\"row_bytes\":"<<row_bytes_<<",\"rows\":"<<rows_<<",\"pixel_bytes\":"<<pixel_bytes_<<",\"file\":\""<<file_<<"\",\"sha256\":\""<<hash_<<"\"";
  if(owned_)o<<",\"owned_copy\":{\"resource\":"<<reinterpret_cast<std::uintptr_t>(owned_)<<",\"copied\":"<<owned_copied_
    <<",\"state_before\":1024,\"state_after\":1024,\"format\":"<<format_<<",\"flags\":0,\"subresource\":0}";
  o<<'}';
}
#ifdef MHW_TEMPORAL_FIXTURE
bool Capture::fixture_release_after_reset(bool safe) noexcept {
  if(attempted_ && !safe)return false;drop(allocator_);drop(list_);drop(source_);drop(buffer_);drop(owned_);drop(device_);retained_=false;return true;
}
#endif
}

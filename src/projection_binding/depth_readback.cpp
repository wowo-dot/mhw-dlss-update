#include "depth_readback.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <ostream>
#include <filesystem>
#include <algorithm>
#include <iomanip>

namespace mhw::projection_binding::depth_readback {
namespace {
template<class T>void drop(T *&p) noexcept {if(p){p->Release();p=nullptr;}}
bool device_matches(ID3D12DeviceChild *child,ID3D12Device *expected) {
  ID3D12Device *device{};const auto hr=child->GetDevice(IID_PPV_ARGS(&device));
  const bool ok=SUCCEEDED(hr) && device==expected;drop(device);return ok;
}
}
bool Capture::extent(unsigned w,unsigned h) noexcept {
  return w>0 && h>0 && w<=8192 && h<=8192 && std::uint64_t(w)*h*4<=byte_limit;
}
bool Capture::source_matches(const D3D12_RESOURCE_DESC &d) const noexcept {
  return d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width==width_ && d.Height==height_ &&
    d.DepthOrArraySize==1 && d.MipLevels==1 && d.Format==DXGI_FORMAT_R32_TYPELESS && d.SampleDesc.Count==1 &&
    d.SampleDesc.Quality==0 && d.Layout==D3D12_TEXTURE_LAYOUT_UNKNOWN && d.Flags==D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}
void Capture::release_unissued() noexcept {
  if(issued())return;
  drop(queue_);drop(allocator_);drop(list_);drop(source_);drop(buffer_);drop(fence_);drop(device_);retained_=false;
}
bool Capture::prepare(ID3D12Device *device,unsigned w,unsigned h) noexcept {
  if(phase()!=Phase::disabled || !device || !extent(w,h))return false;
  width_=w;height_=h;device_=device;device_->AddRef();
  D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;
  d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R32_TYPELESS;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  device_->GetCopyableFootprints(&d,0,1,0,&footprint_,&rows_,&row_bytes_,&bytes_);
  const auto &f=footprint_.Footprint;
  if(footprint_.Offset!=0 || rows_!=h || row_bytes_!=std::uint64_t(w)*4 || f.Width!=w || f.Height!=h || f.Depth!=1 ||
     f.Format!=DXGI_FORMAT_R32_TYPELESS || f.RowPitch%256 || f.RowPitch<row_bytes_ ||
     !bytes_ || bytes_>byte_limit || bytes_!=std::uint64_t(f.RowPitch)*(h-1)+row_bytes_) {
    phase_.store(Phase::failed);release_unissued();return false;
  }
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
  d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes_;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;
  d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if(FAILED(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&buffer_))) ||
     FAILED(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)))) {
    phase_.store(Phase::failed);release_unissued();return false;
  }
  retained_=true;phase_.store(Phase::ready,std::memory_order_release);return true;
}
bool Capture::issue_impl(ID3D12GraphicsCommandList *list,ID3D12Resource *source,const Site &site) {
  site_=site;
  if((site.request_control && (!site.selection || site.marker)) || !list || !source || !site.allocator || !site.reset || !site.barrier || !site.copy || !site.begin ||
     list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT || !source_matches(source->GetDesc()) ||
     !device_matches(source,device_) || !device_matches(list,device_))return false;
  IUnknown *canonical{};
  if(FAILED(source->QueryInterface(IID_PPV_ARGS(&canonical))))return false;
  const bool same=reinterpret_cast<std::uintptr_t>(canonical)==site.identity;drop(canonical);if(!same)return false;
  if(FAILED(source->QueryInterface(IID_PPV_ARGS(&source_))) || source_!=source ||
     FAILED(list->QueryInterface(IID_PPV_ARGS(&list_))) || list_!=list ||
     FAILED(reinterpret_cast<IUnknown *>(site.allocator)->QueryInterface(IID_PPV_ARGS(&allocator_))) ||
     !device_matches(allocator_,device_))return false;
  D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=source_;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.pResource=buffer_;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint_;
  // Set before the void API: even a fault cannot prove no command was recorded.
  attempted_.store(true,std::memory_order_release);
  list_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);copy_returned_=true;return true;
}
bool Capture::issue(ID3D12GraphicsCommandList *list,ID3D12Resource *source,const Site &site) noexcept {
  auto expected=Phase::ready;if(!phase_.compare_exchange_strong(expected,Phase::issuing))return false;
  bool ok=false;__try{ok=issue_impl(list,source,site);}__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  if(!ok)phase_.store(issued()?Phase::quarantined:Phase::failed,std::memory_order_release);
  // Failed qualification can leave partial owning references, but no GPU command.
  if(!ok && !issued())release_unissued();return ok;
}
bool Capture::submit_impl(ID3D12CommandQueue *queue,const Site &site) {
  if(site_.selection) {
    if(site.selection!=site_.selection || site.request_control!=site_.request_control || (!site.marker && !site.request_control) || (site.marker && site.request_control) || site.resource!=site_.resource || site.identity!=site_.identity)return false;
    site_.marker=site.marker;
  }
  if(!queue || queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT || !device_matches(queue,device_) ||
     site.command!=site_.command || site.reset!=site_.reset || !site.close || !site.execute ||
     !(site_.end<site.close && site.close<site.execute && site.execute<site.execute_exit && site.execute_exit<site.signal_begin))return false;
  site_.queue=site.queue;site_.close=site.close;site_.execute=site.execute;site_.execute_exit=site.execute_exit;site_.signal_begin=site.signal_begin;
  if(FAILED(queue->QueryInterface(IID_PPV_ARGS(&queue_))) || queue_!=queue)return false;
#ifdef MHW_BINDING_FIXTURE
  if(signal_failure_){signal_result_=static_cast<unsigned>(E_FAIL);return false;}
#endif
  ++signal_attempts_;signal_result_=static_cast<unsigned>(queue_->Signal(fence_,1));return signal_result_<0x80000000;
}
bool Capture::submit(ID3D12CommandQueue *queue,const Site &site) noexcept {
  auto expected=Phase::recorded;if(!phase_.compare_exchange_strong(expected,Phase::signaling))return false;
  bool ok=false;__try{ok=submit_impl(queue,site);}__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  if(!ok)phase_.store(Phase::quarantined,std::memory_order_release);return ok;
}
void Capture::poll() noexcept {
  if(phase()!=Phase::submitted)return;
  __try{completed_=fence_->GetCompletedValue();poll_tick_=GetTickCount64();}
  __except(EXCEPTION_EXECUTE_HANDLER){phase_.store(Phase::quarantined,std::memory_order_release);return;}
  if(completed_==UINT64_MAX){phase_.store(Phase::quarantined,std::memory_order_release);return;}
  if(completed_==1)phase_.store(Phase::complete,std::memory_order_release);
  else if(completed_>1)phase_.store(Phase::quarantined,std::memory_order_release);
}
bool Capture::terminal() const noexcept {
  const auto p=phase();return (p==Phase::complete && retired_) || p==Phase::saved || p==Phase::failed || p==Phase::quarantined;
}
void Capture::abort() noexcept {
  // Worker-only after hook admission has drained. Never race an issue/signal.
  if(phase()==Phase::disabled || phase()==Phase::saved || (phase()==Phase::complete && retired_))return;
  if(issued())phase_.store(Phase::quarantined,std::memory_order_release);
  else {phase_.store(Phase::failed,std::memory_order_release);release_unissued();}
}
bool Capture::save(const std::wstring &path) {
  if(phase()!=Phase::complete || completed_!=1 || !retired_ || std::filesystem::exists(path))return false;
  void *mapped{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes_)};
#ifdef MHW_BINDING_FIXTURE
  if(map_failure_){map_result_=static_cast<unsigned>(E_FAIL);phase_.store(Phase::failed);return false;}
#endif
  map_result_=static_cast<unsigned>(buffer_->Map(0,&range,&mapped));
  if(map_result_>=0x80000000 || !mapped){phase_.store(Phase::failed);return false;}
  mapped_=true;bool ok=false;
  try {
    std::ofstream out(std::filesystem::path(path),std::ios::binary);
    minimum_=std::numeric_limits<double>::infinity();maximum_=-minimum_;
    for(unsigned y=0;y<height_ && out;++y) {
      auto row=static_cast<const char *>(mapped)+std::size_t(y)*footprint_.Footprint.RowPitch;
      out.write(row,static_cast<std::streamsize>(row_bytes_));
      for(unsigned x=0;x<width_;++x) {
        float value{};std::memcpy(&value,row+std::size_t(x)*4,4);
        if(std::isfinite(value)){++finite_;minimum_=(std::min)(minimum_,double(value));maximum_=(std::max)(maximum_,double(value));
          if(value==0)++zero_;if(value==1)++one_;if(value<0 || value>1)++outside_;}
        else ++nonfinite_;
      }
    }
    out.close();ok=static_cast<bool>(out);
    if(ok)pixel_bytes_=row_bytes_*height_;
  } catch(...) {D3D12_RANGE empty{};buffer_->Unmap(0,&empty);phase_.store(Phase::failed);throw;}
  D3D12_RANGE empty{};buffer_->Unmap(0,&empty);
  phase_.store(ok?Phase::saved:Phase::failed,std::memory_order_release);return ok;
}
void Capture::write(std::ostream &o) const {
  const auto &s=site_;o<<std::boolalpha<<std::setprecision(9)<<"{\"schema\":1,\"phase\":"<<static_cast<unsigned>(phase())
    <<",\"copy_attempted\":"<<issued()<<",\"copy_returned\":"<<copy_returned_<<",\"resources_retained\":"<<retained_
    <<",\"retention\":\"process-lifetime-after-issue\",\"mapped\":"<<mapped_
    <<",\"recording_retired\":"<<retired_<<",\"retire_reset\":"<<retire_reset_<<",\"retire_reset_exit\":"<<retire_exit_
    <<",\"width\":"<<width_<<",\"height\":"<<height_<<",\"format\":39,\"subresource\":0,\"xyz\":[0,0,0],\"has_box\":false"
    <<",\"footprint\":["<<footprint_.Offset<<','<<footprint_.Footprint.Format<<','<<footprint_.Footprint.Width<<','<<footprint_.Footprint.Height
    <<','<<footprint_.Footprint.Depth<<','<<footprint_.Footprint.RowPitch<<"],\"allocation_bytes\":"<<bytes_<<",\"row_bytes\":"<<row_bytes_
    <<",\"rows\":"<<rows_<<",\"pixel_bytes\":"<<pixel_bytes_<<",\"buffer\":"<<reinterpret_cast<std::uintptr_t>(buffer_)
    <<",\"fence\":"<<reinterpret_cast<std::uintptr_t>(fence_)<<",\"fence_value\":1,\"completed_value\":"<<completed_
    <<",\"fence_flags\":0,\"buffer_heap\":3,\"buffer_state\":1024,\"queue_signal_attempts\":"<<signal_attempts_
    <<",\"completion_tick\":"<<poll_tick_<<",\"signal_result\":"<<signal_result_<<",\"map_result\":"<<map_result_
    <<",\"site\":{\"request_control\":"<<s.request_control<<",\"marker\":"<<s.marker<<",\"selection\":"<<s.selection<<",\"reset\":"<<s.reset<<",\"barrier\":"<<s.barrier<<",\"copy\":"<<s.copy
    <<",\"copy_exit\":"<<s.copy_exit<<",\"begin\":"<<s.begin<<",\"end\":"<<s.end<<",\"close\":"<<s.close
    <<",\"execute\":"<<s.execute<<",\"execute_exit\":"<<s.execute_exit<<",\"signal_begin\":"<<s.signal_begin<<",\"signal_end\":"<<s.signal_end
    <<",\"resource\":"<<s.resource<<",\"identity\":"<<s.identity<<",\"command\":"<<s.command<<",\"allocator\":"<<s.allocator<<",\"queue\":"<<s.queue
    <<"},\"pixels\":{\"finite\":"<<finite_<<",\"nonfinite\":"<<nonfinite_<<",\"zero\":"<<zero_<<",\"one\":"<<one_<<",\"outside_unit\":"<<outside_
    <<",\"minimum\":"<<(finite_?minimum_:0)<<",\"maximum\":"<<(finite_?maximum_:0)<<"}}";
}
#ifdef MHW_BINDING_FIXTURE
bool Capture::fixture_release_after_reset(bool reset) noexcept {
  if(issued() && (!reset || completed_!=1))return false;
  // Caller owns the list and establishes Reset after its last GPU submission.
  drop(queue_);drop(allocator_);drop(list_);drop(source_);drop(buffer_);drop(fence_);drop(device_);retained_=false;return true;
}
#endif
}

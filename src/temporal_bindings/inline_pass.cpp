#include "inline_pass.hpp"
#include "owned_inputs.hpp"
#include "submission.hpp"
#include "jitter.hpp"
#include "../dlss_sdk/inline.hpp"
#include "../ngx_startup/startup.hpp"
#include "../temporal_present/observer.hpp"
#include "../projection_binding/recorder.hpp"
#include <wrl/client.h>
#include <bit>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#ifdef MHW_SCALED_OUTPUT
#include "../scaled_temporal/output_route.hpp"
#include "../scaled_temporal/resolve.hpp"
#endif

namespace mhw::temporal_bindings::inline_pass {
namespace {
std::unique_ptr<dlss_sdk::InlineFrame> pass;
Microsoft::WRL::ComPtr<ID3D12Fence> fence;
struct Artifact {std::string name,hash;std::uint64_t bytes{};};
std::array<Artifact,5> artifacts;
bool attempted{},binding_equal{},saved{};
std::string failure;
#ifdef MHW_SCALED_OUTPUT
std::unique_ptr<temporal_present::Output> recovery;
bool claimed{},route_retired{};
continuous_dlaa::ResolveResult resolve_result;
#endif
}
void prepare(ID3D12Device *device,unsigned w,unsigned h,const std::wstring &runtime){
  if(pass || runtime.empty())throw std::runtime_error("inline preparation state");
  const auto *initialization=ngx_startup::borrowed_initialization();
  if(!initialization)throw std::runtime_error("inline startup ownership");
  std::array<ID3D12Resource *,3> raw;for(unsigned i=0;i<3;++i)raw[i]=owned::input(selected_receipt,i);
#ifdef MHW_SCALED_OUTPUT
  const dlss_sdk::Geometry geometry{{w,h},{w/2,h/2},{w,h},dlss_sdk::ReconstructionMode::performance};
  pass=std::make_unique<dlss_sdk::InlineFrame>(device,geometry,raw,runtime,*initialization,true);
  recovery=std::make_unique<temporal_present::Output>(device,temporal_present::SpatialSource{w,h,w/2,h/2},w,h,true);
#else
  pass=std::make_unique<dlss_sdk::InlineFrame>(device,w,h,raw,runtime,*initialization);
#endif
}
bool record(unsigned i,ID3D12GraphicsCommandList *list,ID3D12Resource *native,std::uint64_t frame,std::uint64_t epoch,std::uintptr_t recording) noexcept {
  if(i!=selected_receipt)return true;
  if(!pass || attempted)return false;attempted=true;
  temporal_present::Bindings state;jitter::HistorySample sample;
  if(!temporal_present::observer::sample(i,state) || !jitter::current_history_sample(i,sample) || sample.frame!=frame || sample.epoch!=epoch ||
     !temporal_present::observer::matches_current(list,state))return false;
  if(!submission::inline_begin(i,frame,list,state.generation))return false;
#ifdef MHW_SCALED_OUTPUT
  if(!scaled_temporal::output_route::ready(list,state.generation,frame,recording)){failure="full-output-route-not-ready";submission::inline_end(i,false);return false;}
  claimed=true;
#else
  (void)recording;
#endif
  const dlss_sdk::HistoryFrame history{frame,epoch,std::bit_cast<float>(sample.jitter_bits[0])*.5F,-std::bit_cast<float>(sample.jitter_bits[1])*.5F};
#ifdef MHW_SCALED_OUTPUT
  resolve_result=scaled_temporal::record_resolve(*pass,*recovery,list,native,state,history,true,16.666667F,true,true);
  // A rejected preflight has recorded no GPU commands. It may still use the
  // untouched native dispatch; a partially issued SDK transaction may not.
  if(resolve_result.outcome==continuous_dlaa::ResolveOutcome::rejected&&!resolve_result.sdk.commands)claimed=false;
  const bool ok=resolve_result.output_written();scaled_temporal::output_route::resolved(ok);
#else
  const bool ok=pass->record(list,native,state,history);
#endif
  binding_equal=temporal_present::observer::matches_current(list,state);
  submission::inline_end(i,ok&&binding_equal);return ok&&binding_equal;
}
void completion(ID3D12Fence *value) noexcept {fence.Attach(value);
#ifdef MHW_SCALED_OUTPUT
  route_retired=scaled_temporal::output_route::finish(value);
  if(!route_retired)binding_equal=false;
#endif
}
#ifdef MHW_SCALED_OUTPUT
bool replacement_claimed() noexcept{return claimed;}
#endif
bool save(const std::wstring &prefix){
  if(!pass || !pass->recorded() || !binding_equal || !fence || saved)return false;
  try {
    const auto result=pass->collect(fence.Get(),1,true);
#ifdef MHW_SCALED_OUTPUT
    if(recovery->recorded())recovery->retire(fence.Get(),1,true);
#endif
    const wchar_t *role_names[]{L"color.f32",L"depth.f32",L"motion.f32",L"dlaa.rgba32f",L"native.u32"};
    for(unsigned i=0;i<5;++i){const std::filesystem::path path(prefix+L".inline-"+role_names[i]);
      const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
      if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("inline artifact preservation");
      DWORD written{};const auto bytes=static_cast<DWORD>(result[i].size());
      const bool ok=WriteFile(file,result[i].data(),bytes,&written,nullptr)&&written==bytes;
      const bool flushed=FlushFileBuffers(file)!=0,closed=CloseHandle(file)!=0;
      if(!ok||!flushed||!closed)throw std::runtime_error("inline artifact write");
      artifacts[i]={path.filename().string(),projection_binding::file_sha256(path.wstring()),bytes};
      if(artifacts[i].hash.size()!=64)throw std::runtime_error("inline artifact hash");
    }
    saved=true;fence.Reset();return true;
  }catch(const std::exception &e){failure=e.what();return false;}
}
void write(std::ostream &o){
  o<<"{\"schema\":1,\"receipt\":"<<selected_receipt<<",\"attempted\":"<<attempted<<",\"bindings_equal\":"<<binding_equal<<",\"saved\":"<<saved
   <<",\"failure\":"<<std::quoted(failure)<<",\"pass\":";if(pass)pass->write(o);else o<<"null";
  o<<",\"files\":[";for(unsigned i=0;i<5;++i){if(i)o<<',';const auto &a=artifacts[i];o<<"{\"file\":"<<std::quoted(a.name)<<",\"bytes\":"<<a.bytes<<",\"sha256\":"<<std::quoted(a.hash)<<'}';}o<<']';
#ifdef MHW_SCALED_OUTPUT
  o<<",\"resolve_outcome\":"<<static_cast<unsigned>(resolve_result.outcome)<<",\"full_output_route\":";scaled_temporal::output_route::write(o);
#endif
  o<<'}';
}
}

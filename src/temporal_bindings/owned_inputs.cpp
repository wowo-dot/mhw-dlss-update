#include "owned_inputs.hpp"
#include "consumer.hpp"
#include "depth_routes.hpp"
#include "jitter.hpp"
#include "../temporal_gpu_inputs/live.hpp"
#include "../projection_binding/recorder.hpp"
#include <array>
#include <filesystem>
#include <ostream>
#include <span>
#include <stdexcept>
#include <iomanip>
#include <bit>
#ifdef MHW_TEMPORAL_INLINE
#include "inline_pass.hpp"
#endif
#ifdef MHW_TEMPORAL_DLSS
#include "../dlss_sdk/live.hpp"
#include "../dlss_sdk/contract.hpp"
#include "../ngx_startup/startup.hpp"
#include <fstream>
#endif

namespace mhw::temporal_bindings::owned {
namespace {
namespace tg=mhw::temporal_gpu_inputs;
struct Artifact {std::string file,hash;std::uint64_t bytes{};};
struct Slot {
  std::unique_ptr<tg::Live> live;
  std::array<Artifact,3> files;
  std::uint64_t bytes{},fence{};
  unsigned luid_low{};int luid_high{};
  bool completed{},retired{},saved{};
  std::array<std::uintptr_t,5> objects{};
  std::array<std::uintptr_t,3> outputs{},readbacks{};
};
std::array<Slot,capacity> slots;
bool enabled{},prepared{},qualified{},attempted{},saved{};
unsigned width{},height{};
std::string failure;
#ifdef MHW_TEMPORAL_DLSS
ID3D12Device *sdk_device{};
std::wstring sdk_runtime;
bool sdk_attempted{},sdk_saved{};
std::array<Artifact,capacity> sdk_outputs;
Artifact sdk_events;
#endif
bool save(const std::filesystem::path &p,std::span<const float> values,Artifact &a) {
  a.file=p.filename().string();
  const auto f=CreateFileW(p.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(f==INVALID_HANDLE_VALUE)return false;
  const auto bytes=static_cast<DWORD>(values.size_bytes());DWORD written{};
  const bool ok=WriteFile(f,values.data(),bytes,&written,nullptr) && written==bytes;
  const bool flushed=FlushFileBuffers(f)!=0,closed=CloseHandle(f)!=0;
  if(!ok || !flushed || !closed)return false;
  a.bytes=bytes;a.hash=projection_binding::file_sha256(p.wstring());return a.hash.size()==64;
}
}
bool prepare(ID3D12Device *device,unsigned w,unsigned h,std::string &reason) {
  if(enabled){reason="owned-already-used";return false;}enabled=true;width=w;height=h;
  try {
    for(auto &s:slots){s.live=std::make_unique<tg::Live>(device,w,h);s.bytes=s.live->allocation_bytes();}
    prepared=true;
#ifdef MHW_TEMPORAL_INLINE
    inline_pass::prepare(device,w,h,sdk_runtime);
#endif
#ifdef MHW_TEMPORAL_DLSS
    sdk_device=device; // all three Live objects retain the same device
#endif
    return true;
  }catch(const std::exception &e){failure=e.what();reason="owned-input-allocation: "+failure;return false;}
}
ID3D12Resource *input(unsigned i,unsigned role) noexcept {
  return prepared && i<slots.size() ? slots[i].live->input(role) : nullptr;
}
bool convert_save(const std::wstring &prefix) {
  if(!enabled)return true;
  // Each raw capture proves the shared copy interval, exact generation, producer
  // fence, and recording retirement. Requiring saved images also preserves the
  // independent pixel reference before any owned conversion work is submitted.
  if(!prepared || attempted || !gpu::owned_ready() || !depth_routes::owned_ready())return false;
  qualified=true;attempted=true;
  try {
    for(unsigned i=0;i<slots.size();++i) {
      auto &s=slots[i];const auto result=s.live->convert();
#if defined(MHW_TEMPORAL_DLSS) && !defined(MHW_SCALED_OUTPUT)
      std::string why;
      if(!dlss_sdk::validate({width,height,result.color,result.depth,result.motion},why))throw std::runtime_error(why);
#endif
      s.fence=result.fence_value;s.completed=result.completed;s.retired=result.allocator_retired;
      s.luid_low=result.adapter_low;s.luid_high=result.adapter_high;
      s.objects={result.queue_address,result.list_address,result.allocator_address,result.fence_address,result.heap_address};
      s.outputs=result.output_addresses;s.readbacks=result.readback_addresses;
      const std::array<std::span<const float>,3> values{result.color,result.depth,result.motion};
      constexpr std::array<const wchar_t *,3> role_names{L"color",L"depth",L"motion"};
      for(unsigned j=0;j<3;++j)if(!save(prefix+L".owned-"+std::to_wstring(i)+L"."+role_names[j]+L".f32",values[j],s.files[j]))
        throw std::runtime_error("owned-output-save");
      s.saved=true;
    }
    saved=true;return true;
  }catch(const std::exception &e){failure=e.what();return false;}
}
#ifdef MHW_TEMPORAL_DLSS
void configure_sdk(const std::wstring &runtime){sdk_runtime=runtime;}
bool evaluate_save(const std::wstring &prefix){
  if(!saved||sdk_attempted||!sdk_device||sdk_runtime.empty())return false;
  sdk_attempted=true;
  try {
    const std::filesystem::path events(prefix+L".sdk.jsonl");
    const auto file=CreateFileW(events.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("sdk-log-preserve");
    if(!CloseHandle(file))throw std::runtime_error("sdk-log-close");
    std::ofstream log(events,std::ios::app|std::ios::binary);if(!log)throw std::runtime_error("sdk-log-open");
    std::array<dlss_sdk::GpuFrame,capacity> frames;
    for(unsigned i=0;i<capacity;++i){const auto &s=slots[i];auto &f=frames[i];
      f.width=width;f.height=height;f.producer_fence=s.live->completed_fence();f.producer_value=s.fence;
      f.producer_retired=s.completed&&s.retired&&s.saved;
      for(unsigned j=0;j<3;++j)f.inputs[j]=s.live->converted(j);
    }
    const auto *initialization=ngx_startup::borrowed_initialization();
    if(!initialization)throw std::runtime_error("sdk-startup-ownership-unavailable");
    std::array<dlss_sdk::HistoryFrame,capacity> history;
    for(unsigned i=0;i<capacity;++i){jitter::HistorySample s;
      if(!jitter::history_sample(i,s))throw std::runtime_error("sdk-projection-history-unavailable");
      // The observed projection delta is (native_x/width,native_y/height).
      // D3D viewport conversion makes this (+native_x/2,-native_y/2) pixels.
      history[i]={s.frame,s.epoch,std::bit_cast<float>(s.jitter_bits[0])*.5F,-std::bit_cast<float>(s.jitter_bits[1])*.5F};
    }
    const auto result=dlss_sdk::evaluate_bounded(sdk_device,frames,sdk_runtime,prefix+L".sdk-data",true,log,initialization,history);
    log.close();if(!log)throw std::runtime_error("sdk-log-save");
    sdk_events={events.filename().string(),projection_binding::file_sha256(events.wstring()),std::filesystem::file_size(events)};
    if(!result.complete||!result.feature_released||!result.parameters_destroyed||!result.shutdown_deferred||sdk_events.hash.size()!=64)
      throw std::runtime_error("sdk-result-contract");
    for(unsigned i=0;i<capacity;++i)if(!save(prefix+L".dlaa-"+std::to_wstring(i)+L".rgba32f",result.output[i],sdk_outputs[i]))throw std::runtime_error("sdk-output-save");
    sdk_saved=true;return true;
  }catch(const std::exception &e){failure=e.what();return false;}
}
#endif
void write(std::ostream &o) {
  o<<"{\"enabled\":"<<enabled<<",\"prepared\":"<<prepared<<",\"producer_qualified\":"<<qualified
   <<",\"attempted\":"<<attempted<<",\"saved\":"<<saved<<",\"failure_present\":"<<!failure.empty()
   <<",\"failure\":"<<std::quoted(failure)
   <<",\"width\":"<<width<<",\"height\":"<<height<<",\"native_state_restored\":"<<qualified<<",\"native_bindings_changed\":false"
   <<",\"device_debug_settings_changed\":false,\"sets\":[";
  if(enabled)for(unsigned i=0;i<slots.size();++i){if(i)o<<',';const auto &s=slots[i];
    o<<"{\"receipt\":"<<i<<",\"allocation_bytes\":"<<s.bytes<<",\"fence_value\":"<<s.fence<<",\"completed\":"<<s.completed
     <<",\"allocator_retired\":"<<s.retired<<",\"saved\":"<<s.saved<<",\"adapter_luid_low\":"<<s.luid_low<<",\"adapter_luid_high\":"<<s.luid_high
     <<",\"inputs\":[";for(unsigned j=0;j<3;++j){if(j)o<<',';o<<reinterpret_cast<std::uintptr_t>(input(i,j));}
    o<<"],\"objects\":[";for(unsigned j=0;j<s.objects.size();++j){if(j)o<<',';o<<s.objects[j];}
    o<<"],\"outputs\":[";for(unsigned j=0;j<3;++j){if(j)o<<',';o<<s.outputs[j];}
    o<<"],\"readbacks\":[";for(unsigned j=0;j<3;++j){if(j)o<<',';o<<s.readbacks[j];}
    o<<"],\"input_formats\":[26,41,42],\"output_formats\":[2,41,16],\"srv_mapping\":5768"
     <<",\"command_recipe\":{\"execute\":1,\"signal\":1,\"dispatch\":1,\"copies\":3,\"barriers\":6},\"files\":[";
    for(unsigned j=0;j<3;++j){if(j)o<<',';const auto &f=s.files[j];o<<"{\"file\":\""<<f.file<<"\",\"bytes\":"<<f.bytes<<",\"sha256\":\""<<f.hash<<"\"}";}o<<"]}";
  }
  o<<']';
#ifdef MHW_TEMPORAL_DLSS
  o<<",\"sdk\":{\"attempted\":"<<sdk_attempted<<",\"saved\":"<<sdk_saved<<",\"events\":{\"file\":\""<<sdk_events.file<<"\",\"bytes\":"<<sdk_events.bytes<<",\"sha256\":\""<<sdk_events.hash<<"\"},\"outputs\":[";
  for(unsigned i=0;i<capacity;++i){if(i)o<<',';const auto &a=sdk_outputs[i];o<<"{\"file\":\""<<a.file<<"\",\"bytes\":"<<a.bytes<<",\"sha256\":\""<<a.hash<<"\"}";}o<<"]}";
#endif
  o<<'}';
}
}

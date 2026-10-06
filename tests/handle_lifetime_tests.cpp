#include "handle_lifetime.hpp"
#include "optimal_settings.hpp"
#include <array>
#include <barrier>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace b = mhw::bridge_probe;
using namespace std::chrono_literals;
namespace {
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
b::TransitionInput input(b::LifecycleOperation op, b::ExportId id=b::ExportId::unknown, std::uint64_t token=0) {
  constexpr std::uintptr_t base=0x7ffe00000000ULL, rva=0x1234;
  return {op,{true,true,true,true,true},{id,base,base+rva,rva,0x100000,true,true,true,true,true},token};
}
void initialize(b::OneShotLifecycle &life) {
  require(life.apply(input(b::LifecycleOperation::verify)).after==b::LifecycleState::verified,"verify");
  require(life.apply(input(b::LifecycleOperation::initialize,b::ExportId::d3d12_init)).response==b::BoundaryResponse::success,"init");
  for (int i=0;i<2;++i)
    require(life.apply(input(b::LifecycleOperation::populate,b::ExportId::populate_parameters)).response==b::BoundaryResponse::success,"populate");
}
struct Memory {
  b::CoreCreateRecord record{1,0,0x3000,0x4000,0};
  unsigned reads{},writes{};
  bool writer_fails{},reader_faults{};
};
bool publish(void *p,const std::uint64_t *token) noexcept {
  auto &m=*static_cast<Memory*>(p); ++m.writes;
  if (m.writer_fails) return false;
  m.record.token=*token; return true; // Model the pinned core's immediate copy.
}
b::HandleRecordRead read(void *p,std::uintptr_t address,b::CoreCreateRecord &record) noexcept {
  auto &m=*static_cast<Memory*>(p); ++m.reads;
  if (m.reader_faults) return b::HandleRecordRead::fault;
  if (address!=reinterpret_cast<std::uintptr_t>(&m.record)) return b::HandleRecordRead::rejected;
  record=m.record;return b::HandleRecordRead::success;
}
std::uintptr_t handle(Memory &m) { return reinterpret_cast<std::uintptr_t>(&m.record.token); }
b::UnsignedGetterReply create_u(void *,std::string_view key) noexcept {
  if (key=="Width"||key=="OutWidth") return {1,2560,true,true};
  if (key=="Height"||key=="OutHeight") return {1,1440,true,true};
  if (key=="CreationNodeMask"||key=="VisibilityNodeMask") return {1,1,true,true};
  return {0xBAD00010U,0,true,false};
}
b::SignedGetterReply create_i(void *,std::string_view key) noexcept { return {1,key=="Hint.HDR"?1:0,true,true}; }

void repeated_lifetimes() {
  constexpr auto mode=b::Mode::native_taa_fallback;
  b::OneShotLifecycle life(mode,b::fallback_generation_token(1));initialize(life);
  b::HandleLifetime handles;
  std::array<Memory,2> memory{};
  std::uint64_t previous{},queries{};unsigned unrecorded{};
  for (std::uint32_t generation=1;generation<=256;++generation) {
    require(b::create_lifecycle_ready(mode,life.state()),"recreate gate rejected retired generation");
    require(b::sizing_lifecycle_ready(mode,life.state(),false),"sizing stopped between generations");
    b::CaptureState budget{};
    const auto contract=b::capture_create_contract(budget,{nullptr,create_u,create_i,nullptr,nullptr});
    require(contract.summary.queries_attempted==11 && b::create_contract_allows_evaluate(contract),"per-generation Create contract");
    const auto created=life.apply(input(b::LifecycleOperation::capture_create,b::ExportId::d3d12_create_feature));
    require(created.response==b::BoundaryResponse::success_with_token && created.issued_token>previous,"generation token reused");
    auto &m=memory[generation%2];
    m.record.creation_command_list=0x4000+generation;
    require(handles.issue(mode,created.issued_token,m.record.creation_command_list,publish,&m),"handle publication");
    require(handles.stats().copied_handle==0,"old binding retained");
    require(!handles.issue(mode,created.issued_token,m.record.creation_command_list,publish,&m),"duplicate live Create");
    if (previous) {
      const auto current=m.record.token;
      m.record.token=previous; b::HandleEvidence stale{};
      require(!handles.release(handle(m),read,&m,stale),"stale token retired new generation");
      require(handles.stats().token==current,"stale release corrupted active token");
      m.record.token=current;
    }
    for (int i=0;i<22;++i) {
      const auto policy=b::optimal_settings_call_policy(mode,++queries);
      require(policy!=b::OptimalSettingsCallPolicy::reject && b::sizing_lifecycle_ready(mode,life.state(),true),"sizing stopped at retained capacity");
      if (policy==b::OptimalSettingsCallPolicy::unrecorded) ++unrecorded;
    }
    b::HandleEvidence e{};
    require(handles.read(handle(m),read,&m,e)==created.issued_token && e.record_verified && e.binding_matched,"current record validation");
    Memory foreign=m; e={};
    require(!handles.read(handle(foreign),read,&foreign,e) && !e.read_attempted,"foreign address bypassed binding");
    e={}; const auto retired=handles.release(handle(m),read,&m,e);
    require(retired==created.issued_token,"release");
    require(life.apply(input(b::LifecycleOperation::release,b::ExportId::d3d12_release_feature,retired)).response==b::BoundaryResponse::success,"release policy");
    const auto state=handles.stats();
    require(!state.token&&!state.command_list&&!state.copied_handle,"retired metadata retained");
    const auto reads=m.reads; e={};
    require(!handles.release(handle(m),read,&m,e)&&m.reads==reads,"duplicate release read dead record");
    previous=created.issued_token;
  }
  require(life.state()==b::LifecycleState::released && life.overflowed() && life.events().size()==10 && life.dropped_event_count()>0,"ledger capacity affected operation");
  require(handles.stats().creates==256&&handles.stats().releases==256&&unrecorded==queries-16,"aggregate accounting");
  handles.close();
  require(life.apply(input(b::LifecycleOperation::shutdown,b::ExportId::d3d12_shutdown)).response==b::BoundaryResponse::success,"shutdown after full ledger");
  require(!handles.issue(mode,b::fallback_generation_token(257),0x4000,publish,&memory[0]),"create after shutdown");
}

void rejection_and_cleanup() {
  constexpr auto mode=b::Mode::native_taa_fallback;
  const auto token=b::fallback_generation_token(1);
  for (int mutation=0;mutation<6;++mutation) {
    b::HandleLifetime h; Memory m;
    require(h.issue(mode,token,0x4000,publish,&m),"fixture issue");
    const auto good=m.record;
    if(mutation==0)m.record.operation=2;
    if(mutation==1)m.record.reserved=1;
    if(mutation==2)m.record.parameter_clone=0;
    if(mutation==3)m.record.creation_command_list=0x5000;
    if(mutation==4)m.record.token=b::diagnostic_token(b::ProviderId::isp);
    if(mutation==5)m.record.token=token+1;
    b::HandleEvidence e{};require(!h.release(handle(m),read,&m,e)&&h.stats().token==token,"bad metadata consumed token");
    m.record=good; e={}; require(h.release(handle(m),read,&m,e)==token,"cleanup after invalid record");
  }
  {
    b::HandleLifetime h;Memory m;m.writer_fails=true;
    require(!h.issue(mode,token,0x4000,publish,&m)&&h.stats().closed&&!h.stats().token,"failed publication not closed");
  }
  {
    b::HandleLifetime h;Memory m;require(h.issue(mode,token,0x4000,publish,&m),"fault setup");m.reader_faults=true;
    b::HandleEvidence e{};require(!h.read(handle(m),read,&m,e)&&h.stats().read_faulted,"read fault not sticky");
    m.reader_faults=false;const auto reads=m.reads; e={};
    require(!h.release(handle(m),read,&m,e)&&m.reads==reads,"read after fault latch");
    h.close();require(!h.stats().token&&h.stats().closed,"active shutdown cleanup");
  }
  {
    b::HandleLifetime h;Memory m;require(h.issue(mode,token,0x4000,publish,&m),"cancel setup");
    h.cancel(token+1);require(h.stats().token==token,"stale cancellation affected current generation");
    h.cancel(token);require(!h.stats().token&&h.stats().closed,"failed boundary cancellation");
  }
  for (const auto legacy : {b::Mode::capture_one_evaluate,b::Mode::capture_three_evaluates}) {
    b::OneShotLifecycle life(legacy,token);initialize(life);
    require(life.apply(input(b::LifecycleOperation::capture_create,b::ExportId::d3d12_create_feature)).issued_token==token,"legacy Create");
    require(life.apply(input(b::LifecycleOperation::release,b::ExportId::d3d12_release_feature,token)).response==b::BoundaryResponse::success,"legacy release");
    require(!b::sizing_lifecycle_ready(legacy,life.state(),false),"legacy sizing widened");
    require(life.apply(input(b::LifecycleOperation::capture_create,b::ExportId::d3d12_create_feature)).decision==b::TransitionDecision::invalid_order,"legacy recreated");
  }
  {
    b::OneShotLifecycle life(mode,token);initialize(life);
    life.apply(input(b::LifecycleOperation::capture_create,b::ExportId::d3d12_create_feature));
    require(life.apply(input(b::LifecycleOperation::capture_evaluate,b::ExportId::d3d12_evaluate_feature,token)).after==b::LifecycleState::inert,"unexpected Evaluate revived fallback");
  }
  {
    b::OneShotLifecycle life(mode,token);initialize(life);
    auto bad=input(b::LifecycleOperation::capture_create,b::ExportId::d3d12_create_feature);bad.caller.caller_binary_verified=false;
    require(life.apply(bad).decision==b::TransitionDecision::caller_rejected,"foreign caller accepted");
  }
  static_assert(b::fallback_generation_token(0)==0);
  static_assert(b::fallback_generation_token(b::kFallbackMaxGeneration)==0x1ffffffffULL);
  static_assert(b::fallback_generation_token(b::kFallbackMaxGeneration+1U)==0);
}

struct BlockingRead {
  Memory memory;
  std::promise<void> entered;
  std::shared_future<void> proceed;
};
b::HandleRecordRead blocked_reader(void *ptr,std::uintptr_t address,b::CoreCreateRecord &record) noexcept {
  auto &b=*static_cast<BlockingRead*>(ptr);
  b.entered.set_value(); b.proceed.wait();
  return read(&b.memory,address,record);
}
void concurrent_retirement(bool shutdown) {
  b::HandleLifetime h;BlockingRead context;
  std::promise<void> proceed;context.proceed=proceed.get_future().share();
  const auto token=b::fallback_generation_token(1);
  require(h.issue(b::Mode::native_taa_fallback,token,0x4000,publish,&context.memory),"concurrent issue");
  auto reader=std::async(std::launch::async,[&]{b::HandleEvidence e{};return h.read(handle(context.memory),blocked_reader,&context,e);});
  context.entered.get_future().wait();
  std::promise<void> retiring;
  auto retire=std::async(std::launch::async,[&]{
    retiring.set_value();
    if(shutdown){h.close();return token;}
    b::HandleEvidence e{};return h.release(handle(context.memory),read,&context.memory,e);
  });
  retiring.get_future().wait();
  const bool blocked=retire.wait_for(30ms)==std::future_status::timeout;
  // Always unblock before asserting, including on failure, so no fixture hangs.
  proceed.set_value();
  require(reader.get()==token&&retire.get()==token&&blocked,"retirement raced an active bounded read");
  const auto reads=context.memory.reads;b::HandleEvidence e{};
  require(!h.read(handle(context.memory),read,&context.memory,e)&&context.memory.reads==reads,"retired record read after operation returned");
}
void concurrent_duplicate_release() {
  b::HandleLifetime h; Memory memory; const auto token=b::fallback_generation_token(1);
  require(h.issue(b::Mode::native_taa_fallback,token,0x4000,publish,&memory),"duplicate release setup");
  std::barrier start(3);
  auto release=[&]{start.arrive_and_wait();b::HandleEvidence e{};return h.release(handle(memory),read,&memory,e);};
  auto first=std::async(std::launch::async,release),second=std::async(std::launch::async,release);
  start.arrive_and_wait();
  const auto a=first.get(),c=second.get();
  require((a==token&&c==0)||(a==0&&c==token),"duplicate release consumed twice");
  require(memory.reads==1&&h.stats().releases==1,"duplicate release read retired storage");
}
}
int main() {
  try {
    repeated_lifetimes(); rejection_and_cleanup(); concurrent_retirement(false); concurrent_retirement(true); concurrent_duplicate_release();
    std::cout<<"PASS 256 generations, bounded records, stale/foreign metadata, repeated Create validation, shutdown and concurrent read retirement\n";
    return 0;
  } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}

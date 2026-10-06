#pragma once
#include <thread>
// Exercise the actual native forwarding and shutdown paths, not a model of
// their postconditions. No game process, private shader, or NGX call is used.
namespace scaled_return_fixture {
using namespace fx;
unsigned __declspec(noinline) detour(){return 99;}
void fill_plan(Fixture &f,unsigned slot,std::span<const std::uintptr_t> packets){
 auto &p=route::plans[slot];p.stamp.frame=slot;p.stamp.epoch=slot;p.stamp.context=2;p.stamp.view=3;
 p.temporal.object=packets[0];p.prepared=p.claimed=true;
 auto resource=buffer(f.device.Get(),256,D3D12_HEAP_TYPE_UPLOAD);
 for(auto &value:p.resources)value=resource;
 require(route::prepare_selector(p)&&p.handoff.prepare({slot,slot,2,3},packets)==sf::FrameHandoff::Result::ok,"return fixture plan");
}
void run(Fixture &f,const std::string &mode){
 stream_plan_fixture::Arena code(4096);
 // mov r8, 0x123456789abcdef0; mov eax,42; ret. Native Windows x64 code is
 // permitted to clobber R8; mhw_binding_forward must preserve those results.
 const std::array<unsigned char,16> stub{0x49,0xb8,0xf0,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12,0xb8,42,0,0,0,0xc3};
 std::memcpy(reinterpret_cast<void *>(code.base),stub.data(),stub.size());
 std::memcpy(reinterpret_cast<void *>(code.base+64),stub.data(),stub.size());
 DWORD old{};require(VirtualProtect(reinterpret_cast<void *>(code.base),4096,PAGE_EXECUTE_READ,&old)!=FALSE,"return fixture executable code");
 require(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(code.base),4096)!=FALSE,"return fixture instruction cache");
 route::cfg.device=cd::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());route::begin();
 hr(f.allocator->Reset(),"return allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"return list");
 cd::lists[0]={f.list,1,0,0,false,false};cd::list_count=1;
 if(mode=="return-clobber"){
  std::array<std::byte,16> executor{},context{};std::array<std::byte,0x160> recording{};
  put(executor,8,reinterpret_cast<std::uintptr_t>(recording.data()));put(recording,0x20,reinterpret_cast<std::uintptr_t>(f.list.Get()));
  cd::cfg.backend=1234;put(context,8,cd::cfg.backend);cd::originals[1]=reinterpret_cast<void *>(code.base);
  for(unsigned i=1;i<=2;++i){
   const std::array<std::uintptr_t,1> packets{1000+i};fill_plan(f,i,packets);
   cd::NativeFrame frame{};frame.call.gpr[1]=reinterpret_cast<std::uintptr_t>(executor.data());frame.call.gpr[2]=reinterpret_cast<std::uintptr_t>(context.data());frame.call.gpr[3]=packets[0];
   cd::native(1,&frame);
   require(frame.call.gpr[3]==0x123456789abcdef0ULL&&frame.call.gpr[0]==42,"native return registers preserved");
   require(route::resolve_slot==UINT_MAX&&route::healthy()&&cd::healthy(),"entry packet cleanup despite clobbered return R8");
   require(route::plans[i].resolved&&route::plans[i].handoff.choice()==sf::FrameHandoff::Choice::native,"correct entry frame sealed");
  }
  route::originals[5]=reinterpret_cast<void *>(code.base);
  route::indexed_scope.state=0x88776655;
  for(unsigned i=0;i<2;++i){
   mhw::projection_binding::Frame frame{};frame.gpr[3]=0x100+i;
   route::indexed_dispatch(&frame);
   require(frame.gpr[3]==0x123456789abcdef0ULL&&frame.gpr[0]==42,"indexed native return registers preserved");
   require(route::indexed_scope.sample==UINT_MAX&&route::indexed_scope.state==0x88776655&&route::pending==0,"indexed scope and callback count restored");
  }
  route::indexed_scope={};
  hr(f.list->Close(),"return close");f.messages();
  std::cout<<"PASS real forwarding wrappers: four clobbered-R8 returns, exact native results and cleared resolve/indexed scopes\n";return;
 }
 require(mode=="fault-shutdown","shutdown fixture mode");
 const std::array<std::uintptr_t,2> packets{1111,2222};fill_plan(f,1,packets);auto &p=route::plans[1];
 require(route::retain_recording(p,f.list.Get(),1)&&p.handoff.record(1,packets[1],{reinterpret_cast<std::uintptr_t>(f.list.Get()),1})==sf::FrameHandoff::Result::ok,"incomplete recorded consumer retained");
 for(unsigned i=0;i<2;++i){void *trampoline{};const auto target=code.base+64*i;
  require(MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(detour),&trampoline)==MH_OK&&MH_EnableHook(reinterpret_cast<void *>(target))==MH_OK,"owned shutdown hook installed");
  require(reinterpret_cast<unsigned(*)()>(target)()==99,"owned shutdown detour active");
  if(i){route::targets[0]=target;route::enabled[0]=true;}else{cd::targets[0]=target;cd::enabled[0]=true;}
 }
 require(!route::abandon()&&!route::abandoned,"active admission forbids abandonment");
 route::fail(8);route::stop();route::pending=1;
 require(!route::callbacks_idle()&&!route::abandon()&&!route::abandoned,"inflight callback forbids abandonment");
 // Model a callback returning while the actual stop worker waits. Its pins
 // must remain live throughout; the worker must then restore both owners.
 std::jthread returning([](){Sleep(50);route::pending=0;});
 cd::accepting=true;cd::finish();returning.join();
 require(cd::fault==45&&route::fault==8&&route::abandoned&&!cd::enabled[0]&&!route::enabled[0],"fault preserved and both hook owners disabled");
 for(unsigned i=0;i<2;++i)require(std::memcmp(reinterpret_cast<void *>(code.base+64*i),stub.data(),stub.size())==0&&reinterpret_cast<unsigned(*)()>(code.base+64*i)()==42,"exact original hook bytes and behavior restored");
 require(route::lifetime.live()==1&&!route::lifetime.released()&&!p.resolved&&!p.handoff.submitted_count()&&!route::drained(),"uncertain recording retained, no invented completion");
 require(p.handoff.choice()==sf::FrameHandoff::Choice::native&&!route::owns_packet(packets[0])&&!route::observe_resolve(packets[0],f.list.Get(),1),"fault cutoff seals choice and rejects late callbacks");
 void *value{};D3D12_RANGE range{0,8};hr(p.selector->Map(0,&range,&value),"selector proof");require(*static_cast<const std::uint64_t *>(value)==0,"undecided GPU choice remains native");D3D12_RANGE none{};p.selector->Unmap(0,&none);
 hr(f.list->Close(),"fault close");f.messages();require(MH_Uninitialize()==MH_OK,"owned shutdown fixture removal");
 std::cout<<"PASS fault shutdown: exact hook restoration, sealed native choice, pinned incomplete recording and latched failure\n";
}
}

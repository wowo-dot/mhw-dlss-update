#include "recorder.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace jw=mhw::jitter_writer;
namespace {
std::atomic<unsigned> original_calls{},bad_error{};
bool change_source{};
constexpr std::uintptr_t type=0x12345678, name=0x76543210;
template<class T> void put(void *p,std::size_t at,T value) { std::memcpy(static_cast<std::byte *>(p)+at,&value,sizeof(value)); }
template<class T> T get(const void *p,std::size_t at) { T value{}; std::memcpy(&value,static_cast<const std::byte *>(p)+at,sizeof(value)); return value; }
void check(bool ok,const char *why) { if (!ok) throw std::runtime_error(why); }
struct Object {
  std::array<std::byte,0x188> owner{};
  std::array<std::byte,0x78> record{};
  std::span<std::byte,0x58> descriptor{record.data()+0x10,0x58};
  std::array<std::byte,0x30> header{};
  std::array<std::uint32_t,3> keys{10,20,30};
  std::array<std::uint8_t,3> values{1,2,3};
  Object() {
    put(owner.data(),0,type); put<std::uint8_t>(owner.data(),0x184,1);
    put(descriptor.data(),0,name); put<unsigned>(descriptor.data(),0x10,3);
    put(descriptor.data(),0x18,reinterpret_cast<std::uintptr_t>(owner.data()));
    put(descriptor.data(),0x20,reinterpret_cast<std::uintptr_t>(owner.data()+0x184));
    put(record.data(),0,reinterpret_cast<std::uintptr_t>(header.data()));
    put(record.data(),8,reinterpret_cast<std::uintptr_t>(owner.data()));
    put<unsigned>(record.data(),0x68,11); put<unsigned>(record.data(),0x6c,1);
    put<std::uint16_t>(header.data(),2,3);
    put(header.data(),0x20,reinterpret_cast<std::uintptr_t>(keys.data()));
    put(header.data(),0x28,reinterpret_cast<std::uintptr_t>(values.data()));
  }
};
struct GuardPage {
  void *value{VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS)};
  ~GuardPage() { if (value) VirtualFree(value,0,MEM_RELEASE); }
};
}
extern "C" void writer_fixture_setter(void *,std::uintptr_t);
extern "C" void writer_fixture_returns(void *,std::uintptr_t,std::uint64_t *,std::uint64_t *);
extern "C" const char writer_fixture_return_site;
extern "C" void writer_fixture_body(void *descriptor,std::uintptr_t input) {
  ++original_calls;
  if (GetLastError()!=0x1234) ++bad_error;
  if (descriptor) {
    const auto target=get<std::uintptr_t>(descriptor,0x20);
    if (target) *reinterpret_cast<std::uint8_t *>(target)=static_cast<std::uint8_t>(input);
    if (change_source) {
      const auto header=get<std::uintptr_t>(static_cast<std::byte *>(descriptor)-0x10,0);
      const auto values=get<std::uintptr_t>(reinterpret_cast<void *>(header),0x28);
      *reinterpret_cast<std::uint8_t *>(values+1)=3;
    }
  }
  SetLastError(0x55aa);
}
void invoke(Object &o,unsigned input) {
  std::uint64_t integer{},vector{};
  SetLastError(0x1234);
  writer_fixture_returns(o.descriptor.data(),input,&integer,&vector);
  check(integer==0x1122334455667788ULL && vector==integer,"original return registers changed");
  check(GetLastError()==0x55aa,"outgoing LastError changed");
}
int main(int argc,char **argv) {
  try {
    const std::string mode=argc>1?argv[1]:"roundtrip";
    std::string why;
    check(!jw::host_identity(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),why),"foreign host accepted");
    const bool track_mode=mode.starts_with("track_");
    check(jw::install_owned(reinterpret_cast<std::uintptr_t>(&writer_fixture_setter),
        {name,type,track_mode?reinterpret_cast<std::uintptr_t>(&writer_fixture_return_site):0}),"fixture hook install");
    Object object;
    invoke(object,1); check(jw::state().visits==0,"inactive observer admitted work");
    const auto before_calls=original_calls.load();
    check(jw::arm(mode=="deadline"?GetTickCount64()-jw::window_ms-1:0),"arm");
    check(!jw::arm(),"repeat arm");
    if (mode=="roundtrip") {
      put(object.descriptor.data(),0,name+1); invoke(object,1);
      put(object.descriptor.data(),0,name); put(object.owner.data(),0,type+1); invoke(object,1);
      put(object.owner.data(),0,type); invoke(object,0); invoke(object,1); jw::stop();
      const auto s=jw::state(); check(s.reserved==2 && s.visits==4 && s.named==3 && s.other_type==1,"descriptor filtering");
      for (unsigned n=0;n<2;++n) {
        jw::Call r; check(jw::copy_call(n,r),"record publication");
        check(!r.before.failure && !r.after.failure && r.before.value==1-n && r.after.value==n && r.input==n,"before/original-write/after relation");
      }
      check(original_calls==before_calls+4,"original not forwarded exactly once");
    } else if (mode=="capacity") {
      std::vector<std::thread> threads;
      for (unsigned t=0;t<4;++t) threads.emplace_back([] { Object own; for (unsigned n=0;n<100;++n) invoke(own,n&1); });
      for (auto &t:threads) t.join();
      const auto s=jw::state(); check(s.stop==jw::Stop::capacity && s.reserved==64 && !s.pending && s.dropped,"capacity/publication");
      std::set<std::uint64_t> markers;
      for (unsigned n=0;n<64;++n) {
        jw::Call r; check(jw::copy_call(n,r),"parallel publication");
        check(r.token==n+1 && r.entry_order<r.exit_order && r.after.value==r.input && !r.before.failure && !r.after.failure,"parallel identity/order/value");
        markers.insert(r.entry_order); markers.insert(r.exit_order);
      }
      check(markers.size()==128 && *markers.begin()==1 && *markers.rbegin()==128,"order markers");
      check(original_calls==before_calls+400,"parallel original count");
    } else if (mode=="deadline") {
      invoke(object,0); jw::poll();
      check(jw::state().stop==jw::Stop::deadline && jw::state().reserved==0,"deadline");
    } else if (mode=="invalid") {
      put<unsigned>(object.descriptor.data(),0x10,0x20003); invoke(object,0);
      jw::Call r; check(jw::state().stop==jw::Stop::invalid && jw::copy_call(0,r) && r.before.failure==3 && r.after.failure==3,"unsupported descriptor accepted");
      check(get<std::uint8_t>(object.owner.data(),0x184)==0,"invalid observation suppressed original");
    } else if (mode=="read_failure") {
      SetLastError(0x1234); writer_fixture_setter(nullptr,0);
      check(jw::state().stop==jw::Stop::invalid && jw::state().read_failures==1 && jw::state().reserved==0,"bad read");
    } else if (mode=="track_roundtrip") {
      invoke(object,0);
      put<unsigned>(object.record.data(),0x6c,2); invoke(object,1); jw::stop();
      for (unsigned n=0;n<2;++n) {
        jw::Call r; check(jw::copy_call(n,r),"track publication");
        for (const auto &s:{r.track_before,r.track_after}) {
          check(s.applicable && !s.failure && s.record==reinterpret_cast<std::uintptr_t>(object.record.data()),"track identity");
          check(s.count==3 && s.index==n+1 && s.key_word==(n+2)*10 && s.raw_value==n+2 && (s.raw_value&1)==r.input,"selected key/raw low bit");
        }
        check(r.before.value==1-n && r.after.value==n,"track original write");
      }
      check(original_calls==before_calls+2,"track original count");
    } else if (mode=="track_changed") {
      change_source=true; invoke(object,0); change_source=false; jw::stop();
      jw::Call r; check(jw::copy_call(0,r),"changed track publication");
      check(!r.track_before.failure && !r.track_after.failure && r.track_before.raw_value==2 && r.track_after.raw_value==3 && r.input==0,"changed source was not retained");
    } else if (mode=="track_foreign") {
      put<std::uintptr_t>(object.record.data(),0,1);
      SetLastError(0x1234); writer_fixture_setter(object.descriptor.data(),0); jw::stop();
      jw::Call r; check(jw::copy_call(0,r) && !r.track_before.applicable && !r.track_after.applicable && r.after.value==0,"foreign caller followed as track");
    } else if (mode=="track_bounds" || mode=="track_read_failure" || mode=="track_owner" || mode=="track_overflow") {
      GuardPage page; check(page.value!=nullptr,"guard page allocation");
      if (mode=="track_bounds" || mode=="track_read_failure") put(object.header.data(),0x28,reinterpret_cast<std::uintptr_t>(page.value));
      if (mode=="track_bounds") put<unsigned>(object.record.data(),0x6c,3);
      if (mode=="track_owner") put(object.record.data(),8,reinterpret_cast<std::uintptr_t>(object.owner.data()+8));
      if (mode=="track_overflow") put<std::uintptr_t>(object.header.data(),0x28,UINTPTR_MAX);
      invoke(object,0);
      jw::Call r; check(jw::copy_call(0,r) && jw::state().stop==jw::Stop::invalid,"invalid source did not end observation");
      const unsigned failure=mode=="track_read_failure"?1U:2U;
      check(r.track_before.failure==failure && r.track_after.failure==failure,"track validation/read ordering");
      check(r.after.value==0 && original_calls==before_calls+1,"invalid track suppressed original");
    } else throw std::runtime_error("unknown fixture case");
    check(!bad_error,"incoming LastError changed");
    check(jw::disable(),"disable"); const auto count=original_calls.load();
    invoke(object,1); check(original_calls==count+1,"restored original");
    check(jw::remove_owned(),"owned trampoline cleanup");
    std::cout << "pass: " << mode << '\n'; return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

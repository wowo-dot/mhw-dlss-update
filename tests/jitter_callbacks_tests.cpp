#include "recorder.hpp"
#include <windows.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
std::vector<std::byte> renderer(0x1f570), manager(0x4a50), camera(0x80), layer(0x200), view(0x80);
std::array<std::uintptr_t, 16> layer_vt{}, camera_vt{};
std::uintptr_t renderer_ptr{}, manager_ptr{}, camera_ptr{}, layer_ptr{};
std::atomic<unsigned> update_calls{}, reset_calls{}, setter_calls{}, bad_error{};
std::uint64_t jitter = 0x3f000000be800000;
template<class T> void put(std::vector<std::byte> &b, unsigned at, T value) { std::memcpy(b.data()+at,&value,sizeof(value)); }
void check(bool condition, const char *why) { if (!condition) throw std::runtime_error(why); }
}
extern "C" std::uintptr_t jitter_fixture_update(std::uintptr_t);
extern "C" std::uintptr_t jitter_fixture_reset(std::uintptr_t);
extern "C" std::uintptr_t jitter_fixture_setter(std::uintptr_t,std::uintptr_t);
extern "C" void jitter_fixture_returns(std::uintptr_t,std::uintptr_t,std::uint64_t *,std::uint64_t *);
extern "C" std::uintptr_t jitter_fixture_update_body(std::uintptr_t object) {
  if (object != layer_ptr || GetLastError() != 51) ++bad_error;
  ++update_calls;
  const auto value = jitter_fixture_setter(camera_ptr,reinterpret_cast<std::uintptr_t>(&jitter));
  SetLastError(73); return value + 1;
}
extern "C" std::uintptr_t jitter_fixture_reset_body(std::uintptr_t object) {
  if (object != layer_ptr) ++bad_error;
  ++reset_calls; SetLastError(84); return 0xaabbccdd;
}
extern "C" void jitter_fixture_setter_body(std::uintptr_t object,std::uintptr_t input) {
  if (object != camera_ptr || input != reinterpret_cast<std::uintptr_t>(&jitter)) ++bad_error;
  ++setter_calls;
  // No shared mutation: parallel fixture callers exercise recorder ownership, not game data races.
  SetLastError(91);
}
int main() {
  using namespace mhw::jitter;
  try {
    renderer_ptr=reinterpret_cast<std::uintptr_t>(renderer.data()); manager_ptr=reinterpret_cast<std::uintptr_t>(manager.data());
    camera_ptr=reinterpret_cast<std::uintptr_t>(camera.data()); layer_ptr=reinterpret_cast<std::uintptr_t>(layer.data());
    const auto lvt=reinterpret_cast<std::uintptr_t>(layer_vt.data()), cvt=reinterpret_cast<std::uintptr_t>(camera_vt.data());
    put(layer,0,lvt); put(camera,0,cvt); put(layer,0xc,8U); put(layer,0x14,0x13U);
    put(layer,0x180,0x3f733333U); put(layer,0x184,0x01010000U);
    put(renderer,0x1f450,reinterpret_cast<std::uintptr_t>(view.data())); put(renderer,0x1f210,std::uint64_t{99});
    put(renderer,0x1f208,7U); put(camera,0x40,7U);
    Layout l{reinterpret_cast<std::uintptr_t>(&renderer_ptr),reinterpret_cast<std::uintptr_t>(&camera_ptr),
      reinterpret_cast<std::uintptr_t>(&manager_ptr),lvt,{cvt,cvt}};
    std::string reason;
    check(!install_verified(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),reason),"foreign executable rejected");
    check(install_owned({reinterpret_cast<std::uintptr_t>(&jitter_fixture_update),reinterpret_cast<std::uintptr_t>(&jitter_fixture_reset),
      reinterpret_cast<std::uintptr_t>(&jitter_fixture_setter)},l),"owned hooks installed");
    SetLastError(51); check(jitter_fixture_update(layer_ptr)==0x1122334455667789ULL && GetLastError()==73,"inactive forwarding");
    check(!arm(2),"phase cannot skip menu"); check(arm(1),"menu arm"); check(!arm(1),"active phase cannot reset storage");
    SetLastError(51); check(jitter_fixture_update(layer_ptr)==0x1122334455667789ULL && GetLastError()==73,"nested return and LastError");
    check(jitter_fixture_reset(layer_ptr)==0xaabbccdd && GetLastError()==84,"tail-call original forwarding");
    std::uint64_t integer{}, vector{};
    jitter_fixture_returns(camera_ptr,reinterpret_cast<std::uintptr_t>(&jitter),&integer,&vector);
    check(integer==0x1122334455667788ULL && vector==integer && GetLastError()==91,"RAX and XMM0 preserved");
    poll(state().started + window_ms);
    check(state().stop==Stop::deadline && state().pending==0 && state().reserved==4,"deadline and paired calls");
    Call a,b; check(copy_call(0,a) && copy_call(1,b),"completed rows available");
    check(a.kind==0 && b.kind==2 && b.parent==a.token && a.thread==b.thread,"nested setter attribution");
    check(a.entry_order<b.entry_order && b.entry_order<b.exit_order && b.exit_order<a.exit_order,"nested entry/exit order");
    check(a.before.stable && b.after.stable && !a.before.failure && a.before.reads<=40,"typed bounded snapshots");
    check(b.before.setter_input==jitter && b.before.layer==layer_ptr,"setter input and parent object");
    check(arm(2),"still arm");
    std::vector<std::thread> threads;
    for (unsigned i=0;i<4;++i) threads.emplace_back([] { for (unsigned n=0;n<100;++n) {
      SetLastError(51); const auto r=jitter_fixture_update(layer_ptr); if(r!=0x1122334455667789ULL || GetLastError()!=73) ++bad_error;
    }});
    for(auto &t:threads)t.join();
    check(state().stop==Stop::capacity && state().reserved==capacity && !state().pending,"concurrent bounded capacity");
    for(unsigned i=0;i<capacity;++i) { Call c; check(copy_call(i,c) && c.complete && c.before.reads<=40,"concurrent publication");
      if(c.parent) { Call p; check(copy_call(static_cast<unsigned>(c.parent)-1,p) && p.thread==c.thread,"parent is same thread"); } }
    check(!bad_error && update_calls==402 && reset_calls==1 && setter_calls==403,"all original calls exactly once");
    check(arm(3),"pan arm");
    // Unknown type stops capture while the genuine target still runs normally.
    put(layer,0,std::uintptr_t{1}); SetLastError(51); jitter_fixture_update(layer_ptr);
    check(state().stop==Stop::invalid_sample && state().pending==0,"invalid type fails capture closed");
    put(layer,0,lvt);
    check(remove_owned(),"target bytes restored and owned trampolines removed after callers join");
    SetLastError(51); check(jitter_fixture_update(layer_ptr)==0x1122334455667789ULL && GetLastError()==73,"restored original behavior");
    std::cout << "PASS: real hooks, nested attribution, RAX/XMM0/LastError, exact forwarding, bounded concurrency, deadlines, rejection and removal\n";
    return 0;
  } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}

#include "recorder.hpp"
#include <windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
HMODULE module{};
struct Handle {
  HANDLE value{};
  ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
std::string quote(const std::string &value) {
  std::ostringstream out; out << '"';
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') out << '\\' << c;
    else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c);
    else out << c;
  }
  return out.str() + '"';
}
std::wstring wide(const std::string &s) {
  const auto n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (!n) return {};
  std::wstring result(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), result.data(), n);
  return result;
}
void adapters(std::ostream &o) {
  using Microsoft::WRL::ComPtr;
  ComPtr<IDXGIFactory1> factory;
  const auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  o << "{\"event\":\"adapter_inventory\",\"factory_hr\":" << static_cast<unsigned>(hr) << ",\"adapters\":[";
  if (SUCCEEDED(hr)) {
    bool comma = false;
    for (unsigned i = 0; i < 16; ++i) {
      ComPtr<IDXGIAdapter1> adapter;
      if (factory->EnumAdapters1(i, &adapter) != S_OK) break;
      DXGI_ADAPTER_DESC1 d{}; if (FAILED(adapter->GetDesc1(&d))) break;
      if (comma) o << ','; comma = true;
      o << "{\"vendor\":" << d.VendorId << ",\"device\":" << d.DeviceId << ",\"luid_low\":" << d.AdapterLuid.LowPart
        << ",\"luid_high\":" << d.AdapterLuid.HighPart << ",\"software\":" << ((d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) << '}';
    }
  }
  o << "],\"active_adapter_identified\":false,\"swap_chain_format\":null,\"gpu_instrumented\":false}\n";
}
void snap(std::ostream &o, const mhw::jitter_writer::Snapshot &s) {
  o << "{\"name\":" << s.name << ",\"owner\":" << s.owner << ",\"type\":" << s.type
    << ",\"target\":" << s.target << ",\"flags\":" << s.flags << ",\"index\":" << s.index
    << ",\"value\":" << s.value << ",\"failure\":" << s.failure << '}';
}
void track(std::ostream &o, const mhw::jitter_writer::TrackSnapshot &s) {
  o << "{\"applicable\":" << s.applicable << ",\"record\":" << s.record << ",\"header\":" << s.header
    << ",\"owner\":" << s.owner << ",\"keys\":" << s.keys << ",\"values\":" << s.values
    << ",\"kind\":" << s.kind << ",\"count\":" << s.count << ",\"index\":" << s.index
    << ",\"key_word\":" << s.key_word << ",\"raw_value\":" << s.raw_value << ",\"failure\":" << s.failure << '}';
}
void row(std::ostream &o,const mhw::jitter_writer::Call &r) {
  o << "{\"event\":\"write\",\"token\":" << r.token << ",\"tick\":" << r.tick << ",\"exit_tick\":" << r.exit_tick
    << ",\"entry_order\":" << r.entry_order << ",\"exit_order\":" << r.exit_order
    << ",\"descriptor\":" << r.descriptor << ",\"caller\":" << r.caller << ",\"input\":" << r.input
    << ",\"thread\":" << r.thread << ",\"complete\":" << r.complete << ",\"before\":";
  snap(o,r.before); o << ",\"after\":"; snap(o,r.after);
  o << ",\"track_before\":"; track(o,r.track_before);
  o << ",\"track_after\":"; track(o,r.track_after); o << "}\n";
}
DWORD WINAPI worker(void *) noexcept {
  using namespace mhw::jitter_writer;
  HMODULE pinned{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&worker),&pinned)) return 1;
  try {
    wchar_t dll_path[32768]{},host_path[32768]{};
    const auto dn=GetModuleFileNameW(module,dll_path,static_cast<DWORD>(std::size(dll_path)));
    const auto hn=GetModuleFileNameW(nullptr,host_path,static_cast<DWORD>(std::size(host_path)));
    if (!dn || dn>=std::size(dll_path) || !hn || hn>=std::size(host_path)) return 2;
    const std::filesystem::path dll(dll_path),host(host_path);
    auto config=dll; config+=L".output-dir";
    if (!std::filesystem::exists(config) || std::filesystem::file_size(config)>4096) return 3;
    std::ifstream settings(config,std::ios::binary); std::string folder; std::getline(settings,folder);
    if (!folder.empty() && folder.back()=='\r') folder.pop_back();
    const auto output=std::filesystem::weakly_canonical(std::filesystem::path(wide(folder)));
    if (folder.empty() || !std::filesystem::path(wide(folder)).is_absolute() || !std::filesystem::is_directory(output) ||
        output==std::filesystem::weakly_canonical(host.parent_path())) return 4;
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::string reason; if (!host_identity(base,reason)) return 5;
    FILETIME created_time{},exit{},kernel{},user{};
    if (!GetProcessTimes(GetCurrentProcess(),&created_time,&exit,&kernel,&user)) return 6;
    const auto creation=(static_cast<std::uint64_t>(created_time.dwHighDateTime)<<32)|created_time.dwLowDateTime;
    const auto pid=GetCurrentProcessId();
    const auto path=output/("writer-"+std::to_string(pid)+"-"+std::to_string(creation)+".jsonl");
    { Handle reservation{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
      if (reservation.value==INVALID_HANDLE_VALUE) return 7; }
    std::ofstream log(path,std::ios::binary|std::ios::app); if (!log) return 8;
    log << std::boolalpha << "{\"event\":\"start\",\"schema\":2,\"version\":\"1.1.0\",\"pid\":" << pid
      << ",\"creation\":" << creation << ",\"base\":" << base << ",\"host_sha256\":" << quote(host_sha)
      << ",\"host_version\":\"0.0.0.0\",\"dll_sha256\":" << quote(file_sha256(dll.wstring()))
      << ",\"experiment\":\"typed-jitter-property-writer\",\"provider_substitution\":false,\"record_capacity\":" << capacity
      << ",\"window_ms\":" << window_ms << ",\"visit_limit\":" << visit_limit << "}\n";
    Handle stop_event{},arm_event{};
    const auto prefix="Local\\MhwJitterWriter-v2-"+std::to_string(pid)+"-"+std::to_string(creation)+"-";
    stop_event.value=CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"0").c_str());
    const bool collision0=GetLastError()==ERROR_ALREADY_EXISTS;
    arm_event.value=CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"1").c_str());
    if (!stop_event.value || !arm_event.value || collision0 || GetLastError()==ERROR_ALREADY_EXISTS) {
      log << "{\"event\":\"failed\",\"reason\":\"event-collision\"}\n"; return 9;
    }
    log << "{\"event\":\"ready\",\"hooks_installed\":false}\n" << std::flush;
    const HANDLE handles[]={stop_event.value,arm_event.value};
    bool armed=false,fatal=false;
    const auto ready_tick=GetTickCount64();
    while (!armed && !fatal && GetTickCount64()-ready_tick<1800000) {
      const auto wait=WaitForMultipleObjects(2,handles,FALSE,50);
      if (wait==WAIT_OBJECT_0) break;
      if (wait==WAIT_FAILED) { fatal=true; break; }
      if (wait==WAIT_OBJECT_0+1) {
        if (!log || !install_verified(base,reason) || !arm()) {
          log << "{\"event\":\"failed\",\"reason\":" << quote(reason.empty()?"arm-or-log":reason) << "}\n" << std::flush;
          fatal=true; break;
        }
        adapters(log);
        log << "{\"event\":\"armed\",\"tick\":" << state().started << ",\"live_spans\":5,\"d3d12_module_loaded\":true,\"graphics_api_confirmed\":false}\n" << std::flush;
        armed=true;
      }
    }
    while (armed && !fatal && state().stop==Stop::recording) {
      const auto wait=WaitForMultipleObjects(2,handles,FALSE,25);
      if (wait==WAIT_OBJECT_0) stop();
      if (wait==WAIT_FAILED || wait==WAIT_OBJECT_0+1 || !log) { stop(); fatal=true; }
      poll();
    }
    stop();
    auto s=state(); const auto until=GetTickCount64()+2000;
    while (s.pending && GetTickCount64()<until) { Sleep(10); s=state(); }
    unsigned copied=0;
    if (!s.pending) for (unsigned i=0;i<s.reserved;++i) { Call r; if (copy_call(i,r)) { row(log,r); ++copied; } }
    fatal=fatal || s.pending!=0 || s.stop==Stop::invalid;
    log << "{\"event\":\"capture_complete\",\"records\":" << copied << ",\"reserved\":" << s.reserved
      << ",\"pending\":" << s.pending << ",\"stop\":" << static_cast<unsigned>(s.stop) << ",\"visits\":" << s.visits
      << ",\"named\":" << s.named << ",\"other_type\":" << s.other_type << ",\"dropped\":" << s.dropped
      << ",\"read_failures\":" << s.read_failures << "}\n" << std::flush;
    const bool restored=disable();
    log << "{\"event\":\"stopped\",\"fatal\":" << fatal << ",\"hooks_disabled\":" << restored
      << ",\"pending\":" << state().pending << ",\"trampolines_retained\":true,\"dll_pinned\":true}\n" << std::flush;
    return fatal || !restored || !log ? 10 : 0;
  } catch (...) { disable(); return 11; }
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
  if (reason==DLL_PROCESS_ATTACH) {
    module=instance; DisableThreadLibraryCalls(instance);
    const HANDLE thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
    if (thread) CloseHandle(thread);
  }
  return TRUE;
}

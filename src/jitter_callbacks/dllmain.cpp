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
void snap(std::ostream &o, const mhw::jitter::Snapshot &s) {
  o << '{';
#define FIELD(n) o << "\"" #n "\":" << s.n << ','
  FIELD(failure); FIELD(reads); FIELD(renderer); FIELD(camera); FIELD(manager); FIELD(view); FIELD(layer); FIELD(camera_type); FIELD(layer_type);
  FIELD(frame); FIELD(frame_after); FIELD(epoch); FIELD(epoch_after); FIELD(camera_epoch); FIELD(camera_jitter);
  FIELD(setter_input); FIELD(layer_mask); FIELD(manager_mask); FIELD(view_flags); FIELD(reset_mode);
  FIELD(layer_state); FIELD(layer_flags); FIELD(blend_bits); FIELD(taa_bytes); FIELD(has_layer); FIELD(has_input);
#undef FIELD
  o << "\"stable\":" << s.stable << '}';
}
void row(std::ostream &o, const mhw::jitter::Call &r) {
  o << "{\"event\":\"call\",\"token\":" << r.token << ",\"parent\":" << r.parent << ",\"kind\":" << r.kind
    << ",\"thread\":" << r.thread << ",\"tick\":" << r.tick << ",\"exit_tick\":" << r.exit_tick
    << ",\"entry_order\":" << r.entry_order << ",\"exit_order\":" << r.exit_order << ",\"argument\":" << r.argument
    << ",\"caller\":" << r.caller << ",\"complete\":" << r.complete << ",\"before\":";
  snap(o,r.before); o << ",\"after\":"; snap(o,r.after); o << "}\n";
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
DWORD WINAPI worker(void *) noexcept {
  using namespace mhw::jitter;
  // Inline wrappers and their trampolines may outlive this worker's capture window.
  HMODULE pinned{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
      reinterpret_cast<LPCWSTR>(&worker), &pinned)) return 1;
  try {
    wchar_t dll_path[32768]{}, host_path[32768]{};
    const auto dn = GetModuleFileNameW(module, dll_path, static_cast<DWORD>(std::size(dll_path)));
    const auto hn = GetModuleFileNameW(nullptr, host_path, static_cast<DWORD>(std::size(host_path)));
    if (!dn || dn >= std::size(dll_path) || !hn || hn >= std::size(host_path)) return 2;
    const std::filesystem::path dll(dll_path), host(host_path);
    auto config = dll; config += L".output-dir";
    if (!std::filesystem::exists(config) || std::filesystem::file_size(config) > 4096) return 3;
    std::ifstream settings(config, std::ios::binary); std::string folder; std::getline(settings, folder);
    if (!folder.empty() && folder.back() == '\r') folder.pop_back();
    const auto output = std::filesystem::weakly_canonical(std::filesystem::path(wide(folder)));
    if (folder.empty() || !std::filesystem::path(wide(folder)).is_absolute() || !std::filesystem::is_directory(output) ||
        output == std::filesystem::weakly_canonical(host.parent_path())) return 4;
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::string reason;
    if (!host_identity(base, reason)) return 5;
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 6;
    const auto creation = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    const auto pid = GetCurrentProcessId();
    const auto path = output / ("callbacks-" + std::to_string(pid) + "-" + std::to_string(creation) + ".jsonl");
    { Handle reservation{CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
      if (reservation.value == INVALID_HANDLE_VALUE) return 7; }
    std::ofstream log(path, std::ios::binary | std::ios::app); if (!log) return 8;
    log << std::boolalpha;
    log << "{\"event\":\"start\",\"schema\":1,\"version\":\"1.0.0\",\"pid\":" << pid << ",\"creation\":" << creation
        << ",\"base\":" << base << ",\"host_sha256\":" << quote(host_sha) << ",\"host_version\":\"0.0.0.0\",\"dll_sha256\":"
        << quote(file_sha256(dll.wstring())) << ",\"experiment\":\"native-callbacks-dlss-off\",\"provider_substitution\":false,\"record_capacity\":"
        << capacity << ",\"window_ms\":" << window_ms << "}\n";
    std::array<Handle,4> events;
    std::array<HANDLE,4> handles{};
    for (unsigned p = 0; p < events.size(); ++p) {
      events[p].value = CreateEventW(nullptr, FALSE, FALSE, wide(event_name(pid, creation, p)).c_str());
      const auto error = GetLastError();
      if (!events[p].value || error == ERROR_ALREADY_EXISTS) { log << "{\"event\":\"failed\",\"reason\":\"event-collision\"}\n"; return 9; }
      handles[p] = events[p].value;
    }
    log << "{\"event\":\"ready\",\"next_phase\":1,\"hooks_installed\":false}\n" << std::flush;
    bool installed = false, fatal = false;
    unsigned done = 0, rejected = 0;
    auto idle_since = GetTickCount64();
    while (!fatal && done < 3 && GetTickCount64() - idle_since < 1800000) {
      const auto wait = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, 25);
      if (wait == WAIT_OBJECT_0) break;
      if (wait == WAIT_FAILED) { fatal = true; break; }
      if (wait > WAIT_OBJECT_0 && wait < WAIT_OBJECT_0 + handles.size()) {
        const unsigned p = wait - WAIT_OBJECT_0;
        if (p != done + 1) {
          log << "{\"event\":\"rejected\",\"reason\":\"phase-order\",\"phase\":" << p << "}\n" << std::flush;
          fatal = ++rejected >= 8 || !log; continue;
        }
        if (!log) { fatal = true; break; }
        if (!installed) {
          if (!install_verified(base, reason)) { log << "{\"event\":\"failed\",\"reason\":" << quote(reason) << "}\n" << std::flush; fatal = true; break; }
          installed = true;
          log << "{\"event\":\"installed\",\"live_spans\":7,\"d3d12_module_loaded\":true,\"graphics_api_confirmed\":false}\n";
          adapters(log);
        }
        if (!log) { fatal = true; break; }
        if (!arm(p)) { log << "{\"event\":\"failed\",\"reason\":\"arm-rejected\"}\n" << std::flush; fatal = true; break; }
        log << "{\"event\":\"armed\",\"phase\":" << p << ",\"tick\":" << state().started << "}\n" << std::flush;
        idle_since = GetTickCount64();
      }
      poll();
      auto s = state();
      if (s.phase > done && s.stop != Stop::recording) {
        const auto until = GetTickCount64() + 2000;
        while (s.pending && GetTickCount64() < until) { Sleep(10); s = state(); }
        unsigned copied = 0;
        if (!s.pending) for (unsigned i = 0; i < s.reserved; ++i) { Call r; if (copy_call(i,r)) { row(log,r); ++copied; } }
        log << "{\"event\":\"phase_complete\",\"phase\":" << s.phase << ",\"records\":" << copied << ",\"reserved\":" << s.reserved
            << ",\"pending\":" << s.pending << ",\"dropped\":" << s.dropped << ",\"stop\":" << static_cast<unsigned>(s.stop) << "}\n" << std::flush;
        done = s.phase; idle_since = GetTickCount64();
        fatal = s.pending != 0 || s.stop == Stop::invalid_sample || !log;
        if (!fatal && done < 3) log << "{\"event\":\"ready\",\"next_phase\":" << done + 1 << ",\"hooks_installed\":true}\n" << std::flush;
      }
    }
    const bool restored = disable();
    log << "{\"event\":\"stopped\",\"phases\":" << done << ",\"fatal\":" << fatal << ",\"hooks_disabled\":" << restored
        << ",\"pending\":" << state().pending << ",\"trampolines_retained\":true,\"dll_pinned\":true}\n" << std::flush;
    return fatal || !restored ? 10 : 0;
  } catch (...) { disable(); return 11; }
}
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    module = instance; DisableThreadLibraryCalls(instance);
    const auto thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
  }
  return TRUE;
}

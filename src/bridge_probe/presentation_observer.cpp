#include "presentation_observer.hpp"
#include <d3d12.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cwchar>

namespace mhw::bridge_probe {
namespace {
using Legacy = HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **);
using Hwnd = HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory2 *, IUnknown *, HWND, const DXGI_SWAP_CHAIN_DESC1 *,
                                         const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *, IDXGIOutput *, IDXGISwapChain1 **);
Legacy original_legacy{};
Hwnd original_hwnd{};
struct Event {
  std::atomic_bool ready{};
  PresentationCreation data{};
  bool valid{};
};
std::array<Event, 8> events;
std::atomic_uint reserved{};
std::atomic_bool enabled{}, installed{}, faulted{};
int filter(DWORD code) noexcept {
  if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_IN_PAGE_ERROR) return EXCEPTION_CONTINUE_SEARCH;
  faulted = true;
  enabled = false;
  return EXCEPTION_EXECUTE_HANDLER;
}
void release(IUnknown *object) noexcept {
  if (!object) return;
  __try { object->Release(); } __except (filter(GetExceptionCode())) {}
}
void record(IUnknown *queue_unknown, IDXGISwapChain *chain, IDXGISwapChain1 *chain1, HWND window) noexcept {
  if (!enabled.load(std::memory_order_acquire)) return;
  const auto index = reserved.fetch_add(1, std::memory_order_relaxed);
  if (index >= events.size()) { enabled = false; return; }
  auto &event = events[index];
  auto &s = event.data;
  ID3D12CommandQueue *queue{};
  ID3D12Device *device{};
  bool valid{};
  __try {
    if (SUCCEEDED(queue_unknown->QueryInterface(__uuidof(ID3D12CommandQueue), reinterpret_cast<void **>(&queue))) && queue &&
        SUCCEEDED(queue->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void **>(&device))) && device) {
      s.adapter_luid = device->GetAdapterLuid();
      if (chain1) {
        DXGI_SWAP_CHAIN_DESC1 d{};
        if (SUCCEEDED(chain1->GetDesc1(&d))) {
          s.format = d.Format; s.width = d.Width; s.height = d.Height; s.buffers = d.BufferCount; s.flags = d.Flags;
          valid = true;
        }
      } else if (chain) {
        DXGI_SWAP_CHAIN_DESC d{};
        if (SUCCEEDED(chain->GetDesc(&d))) {
          s.format = d.BufferDesc.Format; s.width = d.BufferDesc.Width; s.height = d.BufferDesc.Height;
          s.buffers = d.BufferCount; s.flags = d.Flags; window = d.OutputWindow;
          valid = true;
        }
      }
    }
  } __except (filter(GetExceptionCode())) { valid = false; }
  release(device);
  release(queue);
  DWORD owner{};
  if (window) GetWindowThreadProcessId(window, &owner);
  event.valid = valid && !faulted && owner == GetCurrentProcessId() &&
                s.format != DXGI_FORMAT_UNKNOWN && s.width && s.height;
  event.ready.store(true, std::memory_order_release);
}
HRESULT STDMETHODCALLTYPE legacy_hook(IDXGIFactory *factory, IUnknown *queue, DXGI_SWAP_CHAIN_DESC *desc,
                                      IDXGISwapChain **out) {
  const auto result = original_legacy(factory, queue, desc, out);
  const auto error = GetLastError();
  if (SUCCEEDED(result) && enabled && out) {
    __try { if (*out) record(queue, *out, nullptr, nullptr); }
    __except (filter(GetExceptionCode())) {}
  }
  SetLastError(error);
  return result;
}
HRESULT STDMETHODCALLTYPE hwnd_hook(IDXGIFactory2 *factory, IUnknown *queue, HWND window,
                                    const DXGI_SWAP_CHAIN_DESC1 *desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen,
                                    IDXGIOutput *restrict_output, IDXGISwapChain1 **out) {
  const auto result = original_hwnd(factory, queue, window, desc, fullscreen, restrict_output, out);
  const auto error = GetLastError();
  if (SUCCEEDED(result) && enabled && out) {
    __try { if (*out) record(queue, nullptr, *out, window); }
    __except (filter(GetExceptionCode())) {}
  }
  SetLastError(error);
  return result;
}
bool system_target(void *target) noexcept {
  MEMORY_BASIC_INFORMATION m{};
  wchar_t actual[MAX_PATH]{}, expected[MAX_PATH]{};
  const auto dxgi = GetModuleHandleW(L"dxgi.dll");
  if (!dxgi || VirtualQuery(target, &m, sizeof(m)) != sizeof(m) ||
      m.AllocationBase != dxgi || m.Type != MEM_IMAGE || m.State != MEM_COMMIT ||
      (m.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
  const auto n = GetSystemDirectoryW(expected, MAX_PATH);
  if (!n || n + 10 >= MAX_PATH || !GetModuleFileNameW(dxgi, actual, MAX_PATH)) return false;
  return wcscat_s(expected, L"\\dxgi.dll") == 0 && _wcsicmp(actual, expected) == 0;
}
} // namespace
bool install_presentation_observer() noexcept {
  if (installed) return true;
  IDXGIFactory2 *factory{};
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&factory))) || !factory) return false;
  void *targets[2]{};
  __try {
    auto table = *reinterpret_cast<void ***>(factory);
    targets[0] = table[10]; targets[1] = table[15];
  } __except (filter(GetExceptionCode())) {}
  release(factory);
  if (faulted || !system_target(targets[0]) || !system_target(targets[1])) return false;
  if (MH_CreateHook(targets[0], reinterpret_cast<void *>(&legacy_hook), reinterpret_cast<void **>(&original_legacy)) != MH_OK)
    return false;
  if (MH_CreateHook(targets[1], reinterpret_cast<void *>(&hwnd_hook), reinterpret_cast<void **>(&original_hwnd)) != MH_OK) {
    MH_RemoveHook(targets[0]); // Never enabled; no callback can own this trampoline.
    return false;
  }
  enabled = true;
  if (MH_EnableHook(targets[0]) != MH_OK || MH_EnableHook(targets[1]) != MH_OK) {
    enabled = false;
    MH_DisableHook(targets[0]); MH_DisableHook(targets[1]);
    // Keep trampolines/module alive if any callback raced enable/disable.
    return false;
  }
  installed = true;
  return true;
}
PresentationCreation presentation_creation(LUID adapter) noexcept {
  PresentationCreation s;
  s.installed = installed;
  s.fault_latched = faulted;
  const auto count = reserved.load(std::memory_order_acquire);
  s.observed = count;
  s.overflow = count > events.size();
  for (unsigned i = 0; i < count && i < events.size(); ++i) {
    if (!events[i].ready.load(std::memory_order_acquire)) { s.in_flight = true; continue; }
    const auto &event = events[i];
    if (!event.valid || event.data.adapter_luid.HighPart != adapter.HighPart ||
        event.data.adapter_luid.LowPart != adapter.LowPart) continue;
    ++s.matching;
    s.format = event.data.format; s.width = event.data.width; s.height = event.data.height;
    s.buffers = event.data.buffers; s.flags = event.data.flags; s.adapter_luid = event.data.adapter_luid;
  }
  s.available = s.installed && !s.fault_latched && !s.overflow && !s.in_flight && s.matching == 1;
  if (!s.available) { s.format = DXGI_FORMAT_UNKNOWN; s.width = s.height = s.buffers = s.flags = 0; }
  return s;
}
} // namespace mhw::bridge_probe

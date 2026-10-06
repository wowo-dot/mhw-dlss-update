#include "depth_record.hpp"
#include "depth_retry.hpp"
#include "depth_tracker.hpp"
#include "capture_retry.hpp"
#include "host_contract.hpp"
#include "../bridge_probe/capture_mailbox.hpp"
#include "../bridge_probe/barrier_journal.hpp"

#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>

#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace ec = mhw::exception_capture;
namespace bp = mhw::bridge_probe;
using ec::Address;
extern "C" Address mhw_depth_fixture_fault(Address unused, const void *backing);
namespace {
constexpr char kHostHash[] =
    "C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA";
std::atomic_bool interrupted{};

struct Handle {
  HANDLE value{};
  explicit Handle(HANDLE v = nullptr) : value(v) {}
  ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
};
void require(bool ok, const char *what) {
  if (!ok) throw std::runtime_error(std::string(what) + " (win32=" +
                                    std::to_string(GetLastError()) + ")");
}
std::string hex(Address n) {
  std::ostringstream s;
  s << "0x" << std::hex << n;
  return s.str();
}

struct Reader {
  HANDLE process;
  unsigned bytes{};
  unsigned calls{};
  bool read(Address address, std::span<std::byte> out, bool code = false) {
    if (!address || out.empty() || out.size() > (code ? 64U : 8U) ||
        address > std::numeric_limits<Address>::max() - out.size() ||
        bytes + out.size() > 4096U || calls >= 256U)
      return false;
    ++calls;
    bytes += static_cast<unsigned>(out.size());
    MEMORY_BASIC_INFORMATION m{};
    if (VirtualQueryEx(process, reinterpret_cast<void *>(address), &m, sizeof(m)) != sizeof(m) ||
        m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
      return false;
    const DWORD p = m.Protect & 0xffU;
    if (p != PAGE_READONLY && p != PAGE_READWRITE && p != PAGE_WRITECOPY &&
        p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY)
      return false;
    if (code && (m.Type != MEM_IMAGE ||
                 (p != PAGE_EXECUTE_READ && p != PAGE_EXECUTE_READWRITE && p != PAGE_EXECUTE_WRITECOPY)))
      return false;
    const auto region = reinterpret_cast<Address>(m.BaseAddress);
    if (address < region || m.RegionSize < out.size() ||
        address - region > m.RegionSize - out.size())
      return false;
    SIZE_T got{};
    return ReadProcessMemory(process, reinterpret_cast<void *>(address), out.data(),
                             out.size(), &got) && got == out.size();
  }
  template <typename T = Address> std::optional<T> scalar(Address address) {
    T n{};
    if (!read(address, std::as_writable_bytes(std::span(&n, 1)))) return {};
    return n;
  }
  static bool callback(void *ctx, Address a, std::span<std::byte> out) {
    return static_cast<Reader *>(ctx)->read(a, out);
  }
};

std::string file_hash(const std::filesystem::path &path) {
  struct Algorithm {
    BCRYPT_ALG_HANDLE h{};
    ~Algorithm() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
  } alg;
  require(BCryptOpenAlgorithmProvider(&alg.h, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0,
          "open SHA256 provider");
  DWORD size{}, got{};
  require(BCryptGetProperty(alg.h, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size),
                           sizeof(size), &got, 0) >= 0, "SHA256 object size");
  std::vector<UCHAR> memory(size);
  struct Hash {
    BCRYPT_HASH_HANDLE h{};
    ~Hash() { if (h) BCryptDestroyHash(h); }
  } hash;
  require(BCryptCreateHash(alg.h, &hash.h, memory.data(), size, nullptr, 0, 0) >= 0,
          "create SHA256 hash");
  std::ifstream input(path, std::ios::binary);
  require(input.is_open(), "open target image for hashing");
  std::array<char, 65536> chunk{};
  while (input.read(chunk.data(), chunk.size()) || input.gcount())
    require(BCryptHashData(hash.h, reinterpret_cast<PUCHAR>(chunk.data()),
                           static_cast<ULONG>(input.gcount()), 0) >= 0, "hash image");
  require(input.eof(), "read target image");
  std::array<UCHAR, 32> digest{};
  require(BCryptFinishHash(hash.h, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0,
          "finish image hash");
  std::string result;
  for (auto b : digest) {
    result += "0123456789ABCDEF"[b >> 4U];
    result += "0123456789ABCDEF"[b & 15U];
  }
  return result;
}

std::filesystem::path process_path(HANDLE process) {
  std::wstring path(32768, L'\0');
  DWORD size = static_cast<DWORD>(path.size());
  require(QueryFullProcessImageNameW(process, 0, path.data(), &size), "target image path");
  path.resize(size);
  return path;
}

bool verify_code(Reader &r, Address base) {
  for (const auto &gate : ec::kCodeGates) {
    std::array<std::byte, 64> expected{}, actual{};
    const auto size = gate.hex.size() / 2U;
    if (size > actual.size()) return false;
    auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < size; ++i)
      expected[i] = static_cast<std::byte>((digit(gate.hex[i * 2U]) << 4) |
                                           digit(gate.hex[i * 2U + 1U]));
    if (!r.read(base + gate.rva, std::span(actual).first(size), true) ||
        std::memcmp(expected.data(), actual.data(), size))
      return false;
  }
  return true;
}

Address verify_host(HANDLE process, DWORD pid) {
  auto path = process_path(process);
  require(_wcsicmp(path.filename().c_str(), L"MonsterHunterWorld.exe") == 0,
          "target executable name rejected");
  require(file_hash(path) == kHostHash, "target executable SHA256 rejected");
  Handle modules(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid));
  require(modules.value != INVALID_HANDLE_VALUE, "target module snapshot");
  MODULEENTRY32W module{};
  module.dwSize = sizeof(module);
  require(Module32FirstW(modules.value, &module), "target main module");
  require(_wcsicmp(module.szModule, L"MonsterHunterWorld.exe") == 0,
          "main module name rejected");
  Address base = reinterpret_cast<Address>(module.modBaseAddr);
  Reader r{process};
  require(r.scalar<std::uint16_t>(base) == 0x5A4DU, "mapped DOS header");
  auto nt = r.scalar<std::uint32_t>(base + 0x3cU);
  require(nt && *nt < 0x1000U, "mapped PE offset");
  require(r.scalar<std::uint32_t>(base + *nt) == 0x4550U &&
          r.scalar<std::uint16_t>(base + *nt + 4) == 0x8664U &&
          r.scalar<std::uint32_t>(base + *nt + 8) == 0x66D13CB6U &&
          r.scalar<std::uint32_t>(base + *nt + 24 + 56) == 0x55D3000U,
          "mapped host identity rejected");
  require(verify_code(r, base), "host code contract rejected");
  return base;
}

struct Output {
  Handle file;
  explicit Output(const std::filesystem::path &path)
      : file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                         CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)) {
    require(file.value != INVALID_HANDLE_VALUE, "create new capture file");
  }
  void write(const std::string &line) {
    const auto text = line + "\n";
    DWORD wrote{};
    require(WriteFile(file.value, text.data(), static_cast<DWORD>(text.size()), &wrote, nullptr) &&
            wrote == text.size(), "write capture");
    require(FlushFileBuffers(file.value), "flush capture");
  }
};
bp::MailboxState save_mailbox(bp::CaptureMailboxReader &mailbox, Output &output,
                             bool &saved, const char *phase, unsigned index = 0) {
  const auto capture = mailbox.read();
  std::string line = "{\"event\":\"bridge-mailbox\",\"phase\":\"" + std::string(phase) +
      "\",\"index\":" + std::to_string(index) + ",\"qualified\":" + (capture.qualified ? "true" : "false") +
      ",\"inputs_verified\":" + (capture.inputs_verified ? "true" : "false") +
      ",\"state\":" + std::to_string(static_cast<unsigned>(capture.state)) +
      ",\"read_only\":true,\"bytes\":" + std::to_string(capture.json.size());
  if (capture.state == bp::MailboxState::published && !saved) line += ",\"capture\":" + capture.json;
  output.write(line + "}");
  saved = saved || capture.state == bp::MailboxState::published;
  return capture.state;
}
bp::JournalRead save_journal(bp::BarrierJournalReader &reader, Output &output, const char *phase, bool stopped) {
  auto capture = reader.read();
  std::string line = "{\"event\":\"barrier-journal\",\"phase\":\"" + std::string(phase) +
      "\",\"read_only\":true,\"target_stopped\":" + (stopped ? "true" : "false") +
      ",\"valid\":" + (capture.valid ? "true" : "false");
  if (capture.valid) line += ",\"capture\":" + capture.json;
  output.write(line + "}"); // Output::write flushes before the debugger continues.
  return capture;
}
std::string snapshot_json(const ec::Snapshot &s, const char *event_name = "depth-record") {
  std::string result = "{\"event\":\"" + std::string(event_name) + "\",\"descriptor_verified\":" +
      std::string(s.descriptor_verified ? "true" : "false") +
      ",\"read_failed\":" + (s.read_failed ? "true" : "false") +
      ",\"budget_exhausted\":" + (s.budget_exhausted ? "true" : "false") +
      ",\"reads\":" + std::to_string(s.reads) + ",\"fields\":{";
  bool first = true;
  for (const auto &f : s.fields) {
    if (!first) result += ',';
    first = false;
    result += '"' + f.name + "\":";
    result += f.value ? ('"' + hex(*f.value) + '"') : "null";
  }
  return result + "}}";
}

struct DebugSession {
  DWORD pid;
  bool attached{};
  explicit DebugSession(DWORD id) : pid(id) {
    require(DebugActiveProcess(pid), "debug attach");
    attached = true;
    if (!DebugSetProcessKillOnExit(FALSE)) {
      DebugActiveProcessStop(pid);
      attached = false;
      throw std::runtime_error("cannot disable debugger kill-on-exit; detached");
    }
  }
  ~DebugSession() { if (attached) DebugActiveProcessStop(pid); }
  bool detach() {
    if (!attached) return true;
    const bool ok = DebugActiveProcessStop(pid) != FALSE;
    if (ok) attached = false;
    return ok;
  }
};
struct PendingEvent {
  DEBUG_EVENT &event;
  DWORD status;
  bool pending{true};
  explicit PendingEvent(DEBUG_EVENT &e) : event(e),
      status(e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE) {}
  ~PendingEvent() {
    if (pending) ContinueDebugEvent(event.dwProcessId, event.dwThreadId, status);
  }
  void resume() {
    require(ContinueDebugEvent(event.dwProcessId, event.dwThreadId, status), "continue debug event");
    pending = false;
  }
};

struct LoopResult {
  bool armed{}, captured{}, detached{}, exited{};
  unsigned events{}, exceptions{}, retries{};
};
enum class ExceptionAction { forward, capture_stop, retry_instruction };
using OnException = std::function<ExceptionAction(const DEBUG_EVENT &)>;
LoopResult watch(DWORD pid, HANDLE process, DWORD duration_ms,
                 const OnException &on_exception, const std::function<void()> &on_ready) {
  DebugSession session(pid);
  LoopResult result;
  const auto deadline = GetTickCount64() + duration_ms;
  while (!interrupted && GetTickCount64() < deadline && result.events < 50000U) {
    DEBUG_EVENT event{};
    if (!WaitForDebugEvent(&event, 100)) {
      require(GetLastError() == ERROR_SEM_TIMEOUT, "wait for debug event");
      continue;
    }
    PendingEvent pending(event);
    bool stop{};
    ++result.events;
    require(event.dwProcessId == pid, "unexpected debug process");
    if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
      Handle close_image(event.u.CreateProcessInfo.hFile);
    } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
      Handle close_image(event.u.LoadDll.hFile);
    } else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
      ++result.exceptions;
      if (!result.armed && event.u.Exception.dwFirstChance &&
          event.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) {
        // Only the attach breakpoint is consumed by default. A separately
        // validated, explicitly requested argument retry is the sole exception.
        pending.status = DBG_CONTINUE;
        pending.resume();
        result.armed = true;
        on_ready();
        continue;
      }
      if (result.armed) {
        const auto action = on_exception(event);
        if (action != ExceptionAction::forward) result.captured = true;
        if (action == ExceptionAction::retry_instruction) {
          pending.status = DBG_CONTINUE;
          ++result.retries;
        }
        stop = action == ExceptionAction::capture_stop;
      }
    } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
      result.exited = true;
      session.attached = false;
    }
    pending.resume();
    if (stop || result.exited) break;
  }
  result.detached = session.detach();
  if (!result.detached && WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    session.attached = false;
    result.exited = true;
  }
  require(result.detached || result.exited, "debug detach failed");
  return result;
}

bool matches_exception(const EXCEPTION_RECORD &e, Address base) {
  return e.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
         reinterpret_cast<Address>(e.ExceptionAddress) == base + 0x22927F2U &&
         e.NumberParameters >= 2U && e.ExceptionInformation[0] == 0U &&
         e.ExceptionInformation[1] == 0x70U;
}

bool same_context(const CONTEXT &a, const CONTEXT &b, Address expected_rdx) {
  return a.Rax == b.Rax && a.Rbx == b.Rbx && a.Rcx == b.Rcx && b.Rdx == expected_rdx &&
      a.Rsi == b.Rsi && a.Rdi == b.Rdi && a.Rbp == b.Rbp && a.Rsp == b.Rsp &&
      a.R8 == b.R8 && a.R9 == b.R9 && a.R10 == b.R10 && a.R11 == b.R11 &&
      a.R12 == b.R12 && a.R13 == b.R13 && a.R14 == b.R14 && a.R15 == b.R15 &&
      a.Rip == b.Rip && a.EFlags == b.EFlags && a.SegCs == b.SegCs && a.SegSs == b.SegSs;
}
// Owns a tentative INTEGER-context update until the event handler has logged
// and accepted it. The debug event holds the thread stopped throughout.
struct ContextEdit {
  HANDLE thread;
  CONTEXT original;
  bool tentative{};
  ContextEdit(HANDLE h, const CONTEXT &ctx) : thread(h), original(ctx) {}
  ~ContextEdit() {
    if (tentative) {
      original.ContextFlags = CONTEXT_INTEGER;
      if (!SetThreadContext(thread, &original))
        std::cerr << "ERROR: failed to restore tentative INTEGER context; exception remains unhandled.\n";
    }
  }
  void apply(Address backing) {
    require(backing != 0 && original.Rdx == 0, "retry argument bounds");
    auto changed = original;
    changed.ContextFlags = CONTEXT_INTEGER;
    changed.Rdx = backing;
    // Attempt rollback even when the API reports failure.
    tentative = true;
    require(SetThreadContext(thread, &changed), "set only depth argument in INTEGER context");
    alignas(16) CONTEXT check{};
    check.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    require(GetThreadContext(thread, &check) && same_context(original, check, backing),
            "verify RDX-only change and unchanged instruction pointer");
  }
  void commit() { tentative = false; }
};

ExceptionAction target_exception(const DEBUG_EVENT &event, HANDLE process, Address base,
                                 Output &out, ec::DepthRetry *retry, bool channels_ready = true,
                                 std::uint64_t capture_deadline = std::numeric_limits<std::uint64_t>::max()) {
  if (!matches_exception(event.u.Exception.ExceptionRecord, base))
    return ExceptionAction::forward;
  Handle thread(OpenThread(THREAD_GET_CONTEXT | (retry ? THREAD_SET_CONTEXT : 0U), FALSE, event.dwThreadId));
  require(thread.value != nullptr, "open fault thread");
  alignas(16) CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  require(GetThreadContext(thread.value, &ctx), "read fault context");
  Reader r{process};
  const auto caller = r.scalar(ctx.Rsp + 8U);
  const bool valid = ctx.Rip == base + 0x22927F2U && ctx.Rdx == 0 && ctx.R8 == 4 &&
                      caller == base + 0x229E89EU && verify_code(r, base);
  out.write("{\"event\":\"matching-exception\",\"first_chance\":" +
      std::to_string(event.u.Exception.dwFirstChance) + ",\"thread\":" +
      std::to_string(event.dwThreadId) + ",\"tick_ms\":" + std::to_string(GetTickCount64()) +
      ",\"contract_verified\":" + (valid ? "true" : "false") +
      ",\"rip\":\"" + hex(ctx.Rip) + "\",\"rdx\":\"" + hex(ctx.Rdx) +
      "\",\"payload\":\"" + hex(ctx.Rbx) + "\",\"descriptor\":\"" + hex(ctx.R15) + "\"}");
  if (valid) {
    const auto snapshot = ec::capture_record(Reader::callback, &r, base, ctx.Rbx, ctx.R15, true);
    out.write(snapshot_json(snapshot));
    const auto tracker = ec::capture_depth_tracker(Reader::callback, &r, snapshot, ctx.Rcx);
    out.write(snapshot_json(tracker.data, "depth-tracker"));
    out.write("{\"event\":\"depth-tracker-decision\",\"complete\":" + std::string(tracker.complete ? "true" : "false") +
        ",\"same_epoch\":" + (tracker.same_epoch ? "true" : "false") +
        ",\"compatible_read_predicted\":" + (tracker.complete && tracker.compatible_read ? "true" : "false") +
        ",\"requested_engine_state\":4,\"gpu_state_verified\":false}");
    if (retry) {
      channels_ready = channels_ready && GetTickCount64() < capture_deadline;
      const auto backing = tracker.complete && channels_ready ?
          retry->take(snapshot, base, event.u.Exception.dwFirstChance != 0, valid,
                      {&tracker, GetTickCount64(), event.dwThreadId}) : std::nullopt;
      if (backing) {
        out.write("{\"event\":\"depth-retry-intent\",\"attempt\":" + std::to_string(retry->attempts()) +
            ",\"rdx_before\":\"0x0\",\"rdx_after\":\"" +
            hex(*backing) + "\",\"rip\":\"" + hex(ctx.Rip) + "\"}");
        ContextEdit edit(thread.value, ctx);
        require(GetTickCount64() < capture_deadline && retry->within_window(GetTickCount64()), "retry deadline before context edit");
        edit.apply(*backing);
        require(GetTickCount64() < capture_deadline && retry->within_window(GetTickCount64()), "retry deadline before commit; rollback on expiry");
        edit.commit();
        out.write("{\"event\":\"depth-retry-applied\",\"context_verified\":true,\"rip_unchanged\":true,"
                  "\"remote_memory_writes\":0,\"retry_budget_remaining\":" + std::to_string(retry->limit() - retry->attempts()) + "}");
        std::cout << "Depth argument correction " << retry->attempts() << '/' << retry->limit()
                  << " verified; retrying the original instruction.\n" << std::flush;
        return ExceptionAction::retry_instruction;
      }
      const auto reason = !channels_ready ? "capture-channel-precondition" :
          !tracker.complete ? "incomplete-tracker" : retry->reason();
      out.write("{\"event\":\"depth-retry-rejected\",\"attempts\":" + std::to_string(retry->attempts()) +
                ",\"reason\":\"" + std::string(reason) + "\"}");
    }
  }
  std::cout << "Matching exception recorded; preserving normal exception handling and detaching.\n" << std::flush;
  return ExceptionAction::capture_stop;
}

BOOL WINAPI control(DWORD code) {
  if (code == CTRL_C_EVENT || code == CTRL_BREAK_EVENT || code == CTRL_CLOSE_EVENT) {
    interrupted = true;
    return TRUE;
  }
  return FALSE;
}

// The fixture contains only fabricated metadata. It is never used to relax
// target checks: --self-test creates and debugs its own child executable.
struct FixtureShared { Address base{}, payload{}, descriptor{}, fault{}, backing{}; unsigned capture_saved{}; };
constexpr Address kFixtureTracker = 0x123456789ABCU;
struct Mapping {
  void *p{};
  explicit Mapping(HANDLE h) : p(MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 4096)) {
    require(p != nullptr, "fixture mapping");
  }
  ~Mapping() { if (p) UnmapViewOfFile(p); }
};
template <typename T, std::size_t N> void put(std::array<std::byte, N> &b, std::size_t off, T value) {
  std::memcpy(b.data() + off, &value, sizeof(value));
}
struct FixtureData {
  std::array<std::byte, 64> payload{}, descriptor{};
  std::array<std::byte, 280> texture{};
  std::array<std::byte, 128> backing{};
  explicit FixtureData(Address base) {
    put(texture, 0, base + 0x351C870U);
    put(texture, 88, std::uint32_t{5});
    put(texture, 96, reinterpret_cast<Address>(backing.data()));
    put(texture, 112, std::uint32_t{0x600000});
    put(texture, 136, std::uint32_t{2560U | (1440U << 16U)});
    put(texture, 144, std::uint32_t{16});
    put(texture, 148, std::uint32_t{0xFFFFFFFF});
    put(backing, 32, Address{1});
    put(backing, 112, kFixtureTracker);
    put(payload, 0, Address{1});
    for (auto offset : {8U, 16U, 24U, 32U})
      put(payload, offset, reinterpret_cast<Address>(texture.data()));
    put(descriptor, 0, std::uint32_t{23});
    put(descriptor, 16, std::uint32_t{3});
    put(descriptor, 32, reinterpret_cast<Address>(payload.data()));
  }
};
bool publish_fixture_capture(bp::CaptureMailboxWriter &mailbox, bp::BarrierJournalWriter &journal,
                             unsigned index, bool overlap) {
  // Owned debugger fixture: metadata is fabricated, corrected load is real.
  const unsigned base = index * 9, slot = 1 + index * 4;
  bp::BarrierCall reset{}; reset.kind = bp::BarrierCallKind::reset; reset.sequence = slot + 1;
  reset.command = 123 + index; reset.entry_order = base + 3; reset.return_order = base + 4; reset.result = S_OK;
  journal.reserve(slot); journal.publish(slot, reset, {}, {});
  bp::BarrierCall crossing = reset; crossing.kind = bp::BarrierCallKind::barrier; crossing.command = 900;
  crossing.sequence = slot + 2; crossing.entry_order = base + 5; crossing.return_order = base + (overlap ? 9 : 6);
  crossing.first = index * 5; crossing.count = 1; journal.reserve(slot + 1);
  D3D12_RESOURCE_BARRIER unrelated{}; unrelated.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  unrelated.Transition = {reinterpret_cast<ID3D12Resource *>(2000ULL + index), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
  if (!overlap) journal.publish(slot + 1, crossing, std::span(&unrelated, 1), {});
  bp::EvaluateContractSnapshot contract{}; contract.summary.complete = true; contract.summary.queries_attempted = bp::kEvaluateQueryCount;
  std::array<D3D12_RESOURCE_BARRIER, 4> transitions{}; unsigned count{};
  for (unsigned i = 0; i < 5; ++i) {
    auto &r = contract.resources[i]; r.key = static_cast<bp::EvaluateResourceKey>(i);
    r.value = i == 2 ? 0 : 1000 + i; r.value_is_null = !r.value;
    r.present = r.query_attempted = r.callback_completed = true; r.getter_result = 1;
    if (r.value) {
      auto &v = transitions[count++]; v.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      v.Transition = {reinterpret_cast<ID3D12Resource *>(r.value), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
          D3D12_RESOURCE_STATE_RENDER_TARGET, i == 4 ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
    }
  }
  bp::BarrierCall own = reset; own.kind = bp::BarrierCallKind::barrier; own.sequence = slot + 3;
  own.entry_order = base + (overlap ? 6 : 7); own.return_order = base + (overlap ? 7 : 8);
  own.first = index * 5 + 1; own.count = 4;
  journal.reserve(slot + 2); journal.publish(slot + 2, own, transitions, {});
  journal.checkpoint(reset.command, GetTickCount64(), base + (overlap ? 8 : 9), slot + 3, overlap ? slot + 1 : slot + 3, index);
  bp::CaptureMetadata m{}; m.sequence = 777; m.thread = GetCurrentThreadId(); m.evaluate_index = index;
  m.committed = m.boundary_clean = m.command_verified = m.handle_record_verified = m.handle_binding_matched = true;
  bp::ResourceInspection inspection{}; inspection.complete = inspection.command_identity_verified = true;
  bp::BarrierCapture barriers{}; barriers.coverage_complete = !overlap; barriers.in_flight = overlap;
  barriers.installed = barriers.armed = barriers.finished = barriers.methods_match = barriers.contract_valid = true;
  barriers.command = reset.command; barriers.evaluate_thread = m.thread;
  if (!mailbox.enter() || !mailbox.publish(m, &contract, &inspection, &barriers, nullptr)) return false;
  if (overlap) journal.publish(slot + 1, crossing, std::span(&unrelated, 1), {});
  bp::BarrierCall execute{}; execute.kind = bp::BarrierCallKind::execute; execute.sequence = slot + 4;
  execute.queue = 456; execute.entry_order = base + 10; execute.return_order = base + 11; execute.first = index; execute.count = 1;
  const std::array<std::uintptr_t, 1> lists{reset.command};
  journal.reserve(slot + 3); journal.publish(slot + 3, execute, {}, lists);
  return true;
}
int fixture_exceptions(unsigned retries, bp::CaptureMailboxWriter *mailboxes, bp::BarrierJournalWriter &journal) {
  unsigned handled{};
  __try { RaiseException(0xE0424201U, 0, 0, nullptr); }
  __except (GetExceptionCode() == 0xE0424201U ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { ++handled; }
  if (retries) {
    journal.start(GetTickCount64() + 2000);
    bp::BarrierCall c{}; c.kind = bp::BarrierCallKind::close; c.sequence = 1; c.command = 123;
    c.entry_order = 1; c.return_order = 2; c.result = S_OK;
    journal.reserve(0); journal.publish(0, c, {}, {});
    // Every admitted read must really execute and return the tracker's value.
    // The terminal fault must reach the application's normal AV handler.
    for (unsigned i = 0; i < retries; ++i) {
      Address value{};
      __try { value = mhw_depth_fixture_fault(0, nullptr); }
      __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { return 59; }
      if (value != kFixtureTracker) return 60;
      if (!publish_fixture_capture(mailboxes[i], journal, i, retries == 3)) return 62;
    }
    __try { mhw_depth_fixture_fault(0, nullptr); }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { ++handled; }
    return handled == 2U ? 57 : 61;
  }
  __try { RaiseException(0xE0424202U, 0, 0, nullptr); }
  __except (GetExceptionCode() == 0xE0424202U ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { ++handled; }
  return handled == 2U ? 57 : 58;
}
int fixture(HANDLE start, HANDLE ready, HANDLE shared, unsigned retries) {
  Mapping mapped(shared);
  std::array<bp::CaptureMailboxWriter, 3> mailboxes;
  bp::BarrierJournalWriter journal;
  if (retries) {
    require(journal.create(), "fixture journal");
    for (unsigned i = 0; i < retries; ++i) require(mailboxes[i].create(i), "fixture capture slot");
  }
  const auto base = reinterpret_cast<Address>(GetModuleHandleW(nullptr));
  FixtureData data(base);
  *static_cast<FixtureShared *>(mapped.p) = {base, reinterpret_cast<Address>(data.payload.data()),
        reinterpret_cast<Address>(data.descriptor.data()), reinterpret_cast<Address>(&mhw_depth_fixture_fault),
        reinterpret_cast<Address>(data.backing.data())};
  require(SetEvent(ready), "fixture ready");
  require(WaitForSingleObject(start, 15000) == WAIT_OBJECT_0, "fixture start wait");
  const auto result = fixture_exceptions(retries, mailboxes.data(), journal);
  if (retries && static_cast<FixtureShared *>(mapped.p)->capture_saved != 1) return 63;
  return result;
}
std::optional<Address> field(const ec::Snapshot &s, const std::string &name) {
  for (const auto &f : s.fields) if (f.name == name) return f.value;
  return {};
}
void set_field(ec::Snapshot &s, const std::string &name, Address value) {
  for (auto &f : s.fields) if (f.name == name) { f.value = value; return; }
  throw std::runtime_error("missing fixture field " + name);
}
ec::TrackerSnapshot retry_tracker_fixture(const ec::Snapshot &s) {
  ec::TrackerSnapshot t{}; t.complete = t.same_epoch = t.data.descriptor_verified = true;
  t.data.fields = {{"anchors.match_work_item", 1}, {"stable", 1}, {"complete", 1}, {"same_epoch", 1},
      {"active.state", 0x800}, {"recheck.active_state", 0x800}, {"owner.primary", field(s, "depth.primary")},
      {"tracker.state", 0x800}, {"recheck.state", 0x800},
      {"backing.native", field(s, "depth.primary.native")}, {"backing.tracker", field(s, "depth.primary.tracker")}};
  return t;
}
void fixture_tracker_state(ec::TrackerSnapshot &tracker, Address state) {
  for (const auto *key : {"active.state", "recheck.active_state", "tracker.state", "recheck.state"})
    set_field(tracker.data, key, state);
  tracker.compatible_read = (state & 4U) != 0;
}
void frame_retry_tests(const ec::Snapshot &input) {
  constexpr Address base = 0x140000000U;
  auto s = input; set_field(s, "renderer.frame", 100);
  auto tracker = retry_tracker_fixture(s);
  const auto mode = ec::DepthRetryMode::three_frames;
  const auto first = [&](ec::DepthRetry &r) {
    require(r.take(s, base, true, true, {&tracker, 1000, 77}).has_value(), "first bounded frame");
  };
  ec::DepthRetry successful(mode); first(successful);
  auto second = s; set_field(second, "renderer.frame", 101);
  require(successful.take(second, base, true, true, {&tracker, 1100, 77}).has_value(), "second consecutive frame");
  auto third = second; set_field(third, "renderer.frame", 102);
  // A freshly validated output buffer may rotate independently of depth.
  set_field(third, "payload.output", *field(third, "payload.output") + 1024);
  set_field(third, "output.primary", 0x5000); set_field(third, "output.primary.recheck", 0x5000);
  set_field(third, "output.primary.native", 0x6000); set_field(third, "output.primary.tracker", 0x7000);
  require(successful.take(third, base, true, true, {&tracker, 1500, 77}).has_value(), "third frame at time limit with rotating output");
  set_field(third, "renderer.frame", 103);
  require(!successful.take(third, base, true, true, {&tracker, 1500, 77}) && successful.attempts() == 3 &&
          successful.reason() == "correction-budget", "fourth correction always rejected");
  require(successful.within_window(1500) && !successful.within_window(1501), "deadline also gates context commit");
  auto read_ready = tracker; fixture_tracker_state(read_ready, 4);
  ec::DepthRetry alternating(mode);
  require(alternating.take(s, base, true, true, {&read_ready, 1000, 77}).has_value() &&
          alternating.take(second, base, true, true, {&tracker, 1100, 77}).has_value(),
          "read-ready first frame and copy-destination second frame both revalidated");
  set_field(third, "renderer.frame", 102);
  require(alternating.take(third, base, true, true, {&read_ready, 1200, 77}).has_value(), "read-ready third frame");
  set_field(third, "renderer.frame", 103);
  require(!alternating.take(third, base, true, true, {&read_ready, 1200, 77}), "read-ready path cannot replenish budget");
  for (const auto state : {0U, 8U, 0xCU, 0xFFU, 0x100U, 0x400U, 0x804U}) {
    auto other = tracker; fixture_tracker_state(other, state);
    ec::DepthRetry r(mode);
    require(!r.take(s, base, true, true, {&other, 1000, 77}), "other engine states remain rejected even with bit 4");
  }
  for (const auto *key : {"tracker.state", "recheck.state", "recheck.active_state"}) {
    auto stale = read_ready; set_field(stale.data, key, 0x800);
    ec::DepthRetry r(mode);
    require(!r.take(s, base, true, true, {&stale, 1000, 77}), "read-ready tracker and active states must agree");
  }
  read_ready.compatible_read = false;
  require(!ec::DepthRetry(mode).take(s, base, true, true, {&read_ready, 1000, 77}), "inconsistent read-ready prediction rejected");
  for (const auto frame : {99ULL, 100ULL, 102ULL, std::numeric_limits<Address>::max()}) {
    ec::DepthRetry r(mode); first(r); auto bad = second; set_field(bad, "renderer.frame", frame);
    require(!r.take(bad, base, true, true, {&tracker, 1100, 77}) && r.attempts() == 1, "backward, duplicate or skipped frame rejected");
  }
  for (const auto observation : {ec::RetryObservation{&tracker, 1501, 77}, {&tracker, 999, 77},
                                 {&tracker, 1100, 78}, {nullptr, 1100, 77}}) {
    ec::DepthRetry r(mode); first(r);
    require(!r.take(second, base, true, true, observation) && r.attempts() == 1, "expired time, changed thread or missing tracker rejected");
  }
  for (const auto name : {"renderer.frame", "renderer", "main_view", "payload.depth", "depth.primary.native"}) {
    auto bad = second;
    for (auto &f : bad.fields) if (f.name == name) f.value.reset();
    ec::DepthRetry r(mode); first(r);
    require(!r.take(bad, base, true, true, {&tracker, 1100, 77}), "incomplete frame identity rejected");
  }
  for (const auto name : {"renderer", "main_view", "main_view.container", "payload.wrapper", "payload.depth",
                          "depth.primary", "depth.primary.native", "depth.primary.tracker", "container.member_144",
                          "container.member_144.secondary", "container.member_144.secondary.native", "container.member_144.secondary.tracker"}) {
    auto bad = second;
    // Change every alias of one object so the per-frame structure stays valid.
    const std::string key(name); const auto value = *field(bad, key) + 0x100000;
    set_field(bad, key, value);
    if (key == "main_view") set_field(bad, "main_view.recheck", value);
    if (key == "main_view.container") set_field(bad, "main_view.container_recheck", value);
    if (key == "payload.depth") { set_field(bad, "container.member_160", value); set_field(bad, "container.member_160.recheck", value); }
    if (key.starts_with("depth.primary")) {
      set_field(bad, "container.member_160" + key.substr(5), value);
      if (key == "depth.primary") { set_field(bad, "depth.primary.recheck", value); set_field(bad, "container.member_160.primary.recheck", value); }
    }
    if (key == "container.member_144" || key == "container.member_144.secondary") set_field(bad, key + ".recheck", value);
    auto changed_tracker = retry_tracker_fixture(bad);
    require(ec::DepthRetry{}.take(bad, base, true, true).has_value(), "changed identity remains individually valid");
    ec::DepthRetry r(mode); first(r);
    require(!r.take(bad, base, true, true, {&changed_tracker, 1100, 77}) && r.reason() == "resource-identity",
            "changed cross-frame identity rejected");
  }
  for (unsigned i = 0; i < 8; ++i) {
    auto bad = tracker;
    if (i == 0) bad.complete = false;
    if (i == 1) bad.same_epoch = false;
    if (i == 2) bad.compatible_read = true;
    if (i == 3) bad.data.read_failed = true;
    if (i == 4) set_field(bad.data, "active.state", 8);
    if (i == 5) set_field(bad.data, "recheck.active_state", 4);
    if (i == 6) bad.data.fields.push_back(bad.data.fields[0]);
    if (i == 7) set_field(bad.data, "backing.native", 0);
    ec::DepthRetry r(mode); first(r);
    require(!r.take(second, base, true, true, {&bad, 1100, 77}), "fresh tracker branch and anchors required");
  }
  bp::JournalRead journal{}; journal.valid = true; journal.state = 2; journal.completed = 100; journal.deadline = 2000;
  require(ec::retry_channels_ready(0, bp::MailboxState::idle, journal, 1000), "first idle mailbox and active journal");
  journal.checkpoint = 2; journal.checkpoint_reserved = journal.checkpoint_prefix = 80;
  journal.evaluate_command = 123; journal.evaluate_order = 161;
  require(ec::retry_channels_ready(1, bp::MailboxState::published, journal, 1000), "subsequent published mailbox and clean checkpoint");
  for (unsigned i = 0; i < 12; ++i) {
    auto bad = journal;
    if (i == 0) bad.valid = false;
    if (i == 1) bad.state = 3;
    if (i == 2) bad.flags = 1;
    if (i == 3) bad.pending = 1;
    if (i == 4) bad.failed = 1;
    if (i == 5) bad.deadline = 1000;
    if (i == 6) bad.checkpoint = 1;
    if (i == 7) --bad.checkpoint_prefix;
    if (i == 8) bad.completed = 79;
    if (i == 9) bad.completed = bp::kBarrierCallCapacity;
    if (i == 10) bad.descriptions = bp::kBarrierDescriptionCapacity;
    if (i == 11) bad.lists = bp::kSubmittedListCapacity;
    require(!ec::retry_channels_ready(1, bp::MailboxState::published, bad, 1000), "channel integrity, checkpoint, time and headroom gates");
  }
  require(!ec::retry_channels_ready(1, bp::MailboxState::entered, journal, 1000) &&
          !ec::retry_channels_ready(0, bp::MailboxState::published, journal, 1000), "wrong mailbox phase rejected");
}
ec::Snapshot local_tests() {
  constexpr Address base = 0x140000000U;
  EXCEPTION_RECORD exception{};
  exception.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  exception.ExceptionAddress = reinterpret_cast<void *>(base + 0x22927F2U);
  exception.NumberParameters = 2;
  exception.ExceptionInformation[1] = 0x70;
  require(matches_exception(exception, base), "exact exception admitted");
  exception.ExceptionInformation[0] = 1;
  require(!matches_exception(exception, base), "write exception rejected");
  exception.ExceptionInformation[0] = 0;
  exception.NumberParameters = 1;
  require(!matches_exception(exception, base), "short exception rejected");
  exception.NumberParameters = 2;
  exception.ExceptionAddress = reinterpret_cast<void *>(base + 0x22927F3U);
  require(!matches_exception(exception, base), "other instruction rejected");
  FixtureData data(base);
  auto capture = [&] {
    Reader r{GetCurrentProcess()};
    return ec::capture_record(Reader::callback, &r, base,
                              reinterpret_cast<Address>(data.payload.data()),
                              reinterpret_cast<Address>(data.descriptor.data()), false);
  };
  auto good = capture();
  require(good.descriptor_verified && !good.read_failed &&
          field(good, "depth.kind") == 5U && field(good, "depth.secondary") == 0U,
          "fixture primary/secondary distinction");
  put(data.descriptor, 16, std::uint32_t{4});
  auto bad = capture();
  require(!bad.descriptor_verified && bad.reads == 3U, "descriptor rejection before owner reads");
  put(data.descriptor, 16, std::uint32_t{3});
  put(data.texture, 0, Address{123});
  auto unknown = capture();
  require(field(unknown, "depth.known_texture_class") == 0U && !field(unknown, "depth.kind"),
          "unknown class must not decode layout");
  void *guard = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  require(guard != nullptr, "allocate guarded fixture");
  DWORD old{};
  require(VirtualProtect(guard, 4096, PAGE_READWRITE | PAGE_GUARD, &old), "protect fixture");
  put(data.payload, 8, reinterpret_cast<Address>(guard));
  auto guarded = capture();
  require(guarded.read_failed && !field(guarded, "color.vtable"), "unreadable is distinct from null");
  MEMORY_BASIC_INFORMATION m{};
  VirtualQuery(guard, &m, sizeof(m));
  require((m.Protect & PAGE_GUARD) != 0U, "guard page must not be touched");
  VirtualFree(guard, 0, MEM_RELEASE);

  // Include the view comparison in schema verification: the same texture
  // appears under two roles, each of which needs distinct owner/slot checks.
  FixtureData view_data(base);
  FixtureData color(base), motion(base), output(base), raw_depth(base);
  put(color.texture, 144, std::uint32_t{34});
  put(color.texture, 112, std::uint32_t{0x200002});
  put(motion.texture, 88, std::uint32_t{0});
  put(motion.texture, 144, std::uint32_t{5});
  put(motion.texture, 112, std::uint32_t{2});
  put(output.texture, 88, std::uint32_t{0});
  put(output.texture, 144, std::uint32_t{34});
  put(output.texture, 112, std::uint32_t{0x600002});
  put(raw_depth.texture, 88, std::uint32_t{6});
  put(raw_depth.texture, 144, std::uint32_t{17});
  put(raw_depth.texture, 112, std::uint32_t{0x500000});
  put(raw_depth.texture, 136, std::uint32_t{0x85A00A00});
  put(raw_depth.texture, 96, Address{0});
  put(raw_depth.texture, 240, reinterpret_cast<Address>(raw_depth.backing.data()));
  put(view_data.payload, 8, reinterpret_cast<Address>(color.texture.data()));
  put(view_data.payload, 16, reinterpret_cast<Address>(motion.texture.data()));
  put(view_data.payload, 24, reinterpret_cast<Address>(output.texture.data()));
  std::array<std::byte, 128088> renderer{};
  std::array<std::byte, 1584> view{};
  std::array<std::byte, 168> container{};
  put(renderer, 128080, reinterpret_cast<Address>(view.data()));
  put(view, 1576, reinterpret_cast<Address>(container.data()));
  put(container, 0, base + 0x35884E0U);
  put(container, 144, reinterpret_cast<Address>(raw_depth.texture.data()));
  put(container, 160, reinterpret_cast<Address>(view_data.texture.data()));
  struct RootReader { Reader reader; Address root; } root_reader{{GetCurrentProcess()}, reinterpret_cast<Address>(renderer.data())};
  auto root_read = [](void *p, Address a, std::span<std::byte> out) {
    auto &r = *static_cast<RootReader *>(p);
    if (a == base + 0x51C4480U && out.size() == sizeof(Address)) {
      std::memcpy(out.data(), &r.root, out.size());
      return true;
    }
    return r.reader.read(a, out);
  };
  auto full = ec::capture_record(root_read, &root_reader, base,
      reinterpret_cast<Address>(view_data.payload.data()), reinterpret_cast<Address>(view_data.descriptor.data()), true);
  require(full.descriptor_verified && !full.read_failed &&
          field(full, "container.member_160.equals_payload_depth") == 1U, "view and payload comparison");
  std::set<std::string> keys;
  for (const auto &f : full.fields) require(keys.insert(f.name).second, "unique JSON field names");
  ec::DepthRetry retry;
  require(!retry.take(full, base, false, true) && !retry.take(full, base, true, false) &&
          retry.attempts() == 0U, "second chance and unverified context rejected without consuming budget");
  require(retry.take(full, base, true, true) == reinterpret_cast<Address>(view_data.backing.data()) &&
          retry.attempts() == 1U && !retry.take(full, base, true, true), "one attempt only");
  for (std::size_t i = 0; i < full.fields.size(); ++i) {
    const auto &name = full.fields[i].name;
    if (name.starts_with("payload.scalar_") || name == "renderer.frame" || name == "main_view.flags") continue;
    auto missing = full;
    missing.fields.erase(missing.fields.begin() + static_cast<std::ptrdiff_t>(i));
    require(!ec::DepthRetry{}.take(missing, base, true, true), ("missing retry invariant rejected: " + name).c_str());
    auto unreadable = full;
    unreadable.fields[i].value.reset();
    require(!ec::DepthRetry{}.take(unreadable, base, true, true), "unreadable retry invariant rejected");
  }
  for (const auto *key : {"depth.kind", "depth.flags", "depth.format_id", "depth.packed_dimensions",
                          "depth.secondary", "depth.primary.recheck", "depth.primary.tracker",
                          "container.member_160", "container.member_144.kind", "container.member_144.primary",
                          "main_view.stable", "descriptor.stable", "color.kind", "output.flags"}) {
    auto wrong = full;
    for (auto &f : wrong.fields) if (f.name == key) f.value = (*f.value == 0 ? 1 : 0);
    require(!ec::DepthRetry{}.take(wrong, base, true, true), "changed retry invariant rejected");
  }
  auto duplicate = full;
  duplicate.fields.push_back(full.fields.front());
  require(!ec::DepthRetry{}.take(duplicate, base, true, true), "duplicate retry fields rejected");
  for (const auto member : {&ec::Snapshot::read_failed, &ec::Snapshot::budget_exhausted}) {
    auto failed = full;
    failed.*member = true;
    require(!ec::DepthRetry{}.take(failed, base, true, true), "incomplete capture rejected");
  }
  auto unverified = full;
  unverified.descriptor_verified = false;
  require(!ec::DepthRetry{}.take(unverified, base, true, true), "unverified descriptor rejected");
  return full;
}
void debug_fixture_test(bool capture_expected, const ec::Snapshot *retry_fixture = nullptr, bool three_frames = false) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  Handle start(CreateEventW(&sa, TRUE, FALSE, nullptr));
  Handle ready(CreateEventW(&sa, TRUE, FALSE, nullptr));
  Handle shared(CreateFileMappingW(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, 4096, nullptr));
  require(start.value && ready.value && shared.value, "fixture handles");
  Mapping mapped(shared.value);
  auto path = process_path(GetCurrentProcess());
  std::wstring command = L"\"" + path.wstring() + (three_frames ? L"\" --three-frame-fixture " :
      retry_fixture ? L"\" --retry-fixture " : L"\" --fixture ") +
      std::to_wstring(reinterpret_cast<Address>(start.value)) + L" " +
      std::to_wstring(reinterpret_cast<Address>(ready.value)) + L" " +
      std::to_wstring(reinterpret_cast<Address>(shared.value));
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION child{};
  require(CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, TRUE,
                         CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child), "create fixture child");
  Handle child_process(child.hProcess), child_thread(child.hThread);
  require(WaitForSingleObject(ready.value, 5000) == WAIT_OBJECT_0, "wait fixture ready");
  const auto f = *static_cast<FixtureShared *>(mapped.p);
  std::array<bp::CaptureMailboxReader, 3> mailboxes;
  auto &mailbox = mailboxes[0];
  bp::BarrierJournalReader journal;
  std::unique_ptr<Output> mailbox_output;
  std::filesystem::path mailbox_path;
  std::array<bool, 3> captures_saved{};
  auto &mailbox_saved = captures_saved[0];
  if (retry_fixture) {
    require(mailbox.open(child.dwProcessId, bp::process_creation_stamp(child_process.value)) &&
            mailbox.read().state == bp::MailboxState::idle, "fixture reader holds mailbox before retry");
    if (three_frames) for (unsigned i = 1; i < 3; ++i)
      require(mailboxes[i].open(child.dwProcessId, bp::process_creation_stamp(child_process.value), i), "fixture all immutable slots open");
    require(journal.open(child.dwProcessId, bp::process_creation_stamp(child_process.value)) &&
            journal.read().valid && journal.read().state == 1, "fixture reader holds idle journal before retry");
    const auto directory = std::filesystem::current_path() / "local" / "mailbox-fixtures";
    std::filesystem::create_directories(directory);
    mailbox_path = directory / ("debug-" + std::to_string(child.dwProcessId) + "-" + std::to_string(GetTickCount64()) + ".jsonl");
    mailbox_output = std::make_unique<Output>(mailbox_path);
  }
  unsigned ordinary{}, faults{};
  ec::DepthRetry retry(three_frames ? ec::DepthRetryMode::three_frames : ec::DepthRetryMode::once);
  ec::Snapshot policy;
  if (retry_fixture) {
    policy = *retry_fixture;
    // Fabricated metadata only; actual context correction uses this child's
    // real backing address and its unchanged, image-backed load instruction.
    for (auto &v : policy.fields)
      if (v.name == "depth.primary" || v.name == "depth.primary.recheck" ||
          v.name == "container.member_160.primary" || v.name == "container.member_160.primary.recheck")
        v.value = f.backing;
  }
  const auto result = watch(child.dwProcessId, child.hProcess, capture_expected ? 5000U : 500U,
    [&](const DEBUG_EVENT &event) {
      const DWORD code = event.u.Exception.ExceptionRecord.ExceptionCode;
      if (code == 0xE0424201U) ++ordinary;
      if (retry_fixture && code == EXCEPTION_ACCESS_VIOLATION) {
        const auto &e = event.u.Exception.ExceptionRecord;
        require(event.u.Exception.dwFirstChance && reinterpret_cast<Address>(e.ExceptionAddress) == f.fault &&
                e.NumberParameters == 2U && e.ExceptionInformation[0] == 0U && e.ExceptionInformation[1] == 0x70U,
                "owned child fault is exact original null load");
        ++faults;
        Handle thread(OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, event.dwThreadId));
        alignas(16) CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        require(thread.value && GetThreadContext(thread.value, &ctx) && ctx.Rdx == 0 && ctx.Rip == f.fault,
                "owned child retry context");
        set_field(policy, "renderer.frame", 100 + faults);
        auto tracker = retry_tracker_fixture(policy);
        if (three_frames && (faults % 2U)) fixture_tracker_state(tracker, 4);
        const auto status = save_mailbox(mailbox, *mailbox_output, mailbox_saved, "matching-exception-before-continue");
        std::array<bp::MailboxRead, 3> captures{}; captures[0] = mailbox.read();
        if (three_frames) for (unsigned i = 1; i < 3; ++i) {
          save_mailbox(mailboxes[i], *mailbox_output, captures_saved[i], "matching-exception-before-continue", i);
          captures[i] = mailboxes[i].read();
        }
        const auto trace = save_journal(journal, *mailbox_output, "matching-exception-before-continue", true);
        require(trace.completed == 1 + 4 * (faults - 1) && (three_frames ? ec::retry_sequence_ready(retry.attempts(), captures, trace, GetTickCount64()) :
            ec::retry_channels_ready(retry.attempts(), status, trace, GetTickCount64())),
                "each owned fault saved and checked before context change");
        auto backing = retry.take(policy, 0x140000000U, true, true, {&tracker, GetTickCount64(), event.dwThreadId});
        if (!backing) {
          require(status == bp::MailboxState::published && mailbox_saved, "capture saved while terminal AV is stopped");
          std::ifstream saved(mailbox_path, std::ios::binary);
          std::string persisted((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
          require(persisted.find("\"capture\":{\"schema\":1,\"producer\":1500,\"evaluate_index\":0,\"sequence\":777") != std::string::npos,
                  "published capture already readable from flushed output before continuing fault");
          if (three_frames) for (unsigned i = 0; i < 3; ++i)
            require(captures_saved[i] && persisted.find("\"evaluate_index\":" + std::to_string(i) + ",\"sequence\":777") != std::string::npos,
                    "each immutable capture flushed before final fault continuation");
          require(trace.completed == 1 + 4 * retry.limit() && trace.pending == 0 && trace.flags == 0, "all completed fixture submissions saved before terminal AV");
          require(persisted.find("\"lists\":[123]") != std::string::npos &&
                  persisted.find("\"evaluate_order\":" + std::to_string(three_frames ? 8 : 9)) != std::string::npos &&
                  persisted.find("\"completed\":" + std::to_string(1 + 4 * retry.limit())) != std::string::npos,
                  "post-checkpoint final submission already flushed");
          static_cast<FixtureShared *>(mapped.p)->capture_saved = 1; // Owned test protocol only.
          return ExceptionAction::capture_stop;
        }
        require(faults <= retry.limit() && backing == f.backing, "bounded owned child fault admitted");
        // Verify rollback independently before accepting the real retry.
        { ContextEdit rollback(thread.value, ctx); rollback.apply(*backing); }
        alignas(16) CONTEXT restored{};
        restored.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        require(GetThreadContext(thread.value, &restored) && same_context(ctx, restored, 0), "tentative edit rollback");
        ContextEdit edit(thread.value, ctx);
        edit.apply(*backing);
        edit.commit();
        return ExceptionAction::retry_instruction;
      }
      if (code != 0xE0424202U) return ExceptionAction::forward;
      Handle fault_thread(OpenThread(THREAD_GET_CONTEXT, FALSE, event.dwThreadId));
      alignas(16) CONTEXT ctx{};
      ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
      require(fault_thread.value && GetThreadContext(fault_thread.value, &ctx) && ctx.Rip && ctx.Rsp,
              "read suspended fixture thread context");
      Reader r{child.hProcess};
      auto s = ec::capture_record(Reader::callback, &r, f.base, f.payload, f.descriptor, false);
      require(s.descriptor_verified && !s.read_failed && field(s, "depth.kind") == 5U &&
              field(s, "depth.secondary") == 0U, "remote event snapshot");
      return ExceptionAction::capture_stop;
    }, [&] { if (capture_expected) require(SetEvent(start.value), "release fixture"); });
  require(result.armed && result.detached && result.captured == capture_expected, "capture/detach result");
  if (capture_expected) require(ordinary == 1U, "unrelated exception forwarded");
  if (retry_fixture) require(faults == retry.limit() + 1 && result.retries == retry.limit() && retry.attempts() == retry.limit(),
                             "bounded corrected instructions then normal terminal AV handling");
  require(SetEvent(start.value), "release detached fixture");
  require(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0, "fixture completion after detach");
  DWORD exit{};
  require(GetExitCodeProcess(child.hProcess, &exit) && exit == 57U, "both application handlers survived");
  if (retry_fixture) require(mailbox.read().state == bp::MailboxState::published,
                              "reader keeps capture after fixture exit");
  if (retry_fixture) require(journal.read().valid && journal.read().completed == 1 + 4 * retry.limit(), "journal retained after fixture exit");
  if (three_frames) for (unsigned i = 0; i < 3; ++i)
    require(mailboxes[i].read().inputs_verified && !mailboxes[i].read().qualified &&
            bp::review_checkpoint(mailboxes[i].read(), journal.read(), i).accepted && journal.read().checkpoints[i].state == 2,
            "all three captures and checkpoints retained after producer exit");
  if (retry_fixture) std::cout << "PASS owned debugger fixture: corrections=" << retry.attempts() << " faults=" << faults
      << " journal_calls=" << journal.read().completed << " terminal_forwarded=true detached=true exit=" << exit << '\n';
}

std::uint64_t number(const wchar_t *s) {
  std::size_t end{};
  auto n = std::stoull(s, &end, 10);
  require(end == std::wcslen(s), "invalid numeric argument");
  return n;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  try {
    if (argc == 5 && (std::wstring_view(argv[1]) == L"--fixture" || std::wstring_view(argv[1]) == L"--retry-fixture" ||
                     std::wstring_view(argv[1]) == L"--three-frame-fixture"))
      return fixture(reinterpret_cast<HANDLE>(number(argv[2])), reinterpret_cast<HANDLE>(number(argv[3])),
                     reinterpret_cast<HANDLE>(number(argv[4])), std::wstring_view(argv[1]) == L"--three-frame-fixture" ? 3U :
                     std::wstring_view(argv[1]) == L"--retry-fixture" ? 1U : 0U);
    if (argc == 2 && std::wstring_view(argv[1]) == L"--self-test") {
      const auto policy_fixture = local_tests();
      frame_retry_tests(policy_fixture);
      debug_fixture_test(true);
      debug_fixture_test(false);
      debug_fixture_test(true, &policy_fixture);
      debug_fixture_test(true, &policy_fixture, true);
      std::cout << "PASS: bounded reads, guard pages, forwarding, detach, fail-closed frame/identity/time/tracker/channel policy, context rollback, one/three RDX-only retries, capture flushed before terminal AV forwarding, and retention after fixture exit.\n";
      return 0;
    }
    const bool once = argc == 5 && std::wstring_view(argv[4]) == L"--repair-depth-once";
    const bool three_frames = argc == 5 && std::wstring_view(argv[4]) == L"--repair-depth-three-frames";
    const bool repair = once || three_frames;
    if (argc != 4 && !repair) {
      std::cerr << "Usage: mhw_depth_exception_capture <pid> <new-output.jsonl> <seconds:1..180> [--repair-depth-once|--repair-depth-three-frames]\n";
      return 2;
    }
    const auto pid64 = number(argv[1]), seconds = number(argv[3]);
    require(pid64 > 0 && pid64 <= MAXDWORD && seconds >= 1 && seconds <= 180, "argument bounds");
    const auto pid = static_cast<DWORD>(pid64);
    Handle process(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid));
    require(process.value != nullptr, "open target for read access");
    const auto base = verify_host(process.value, pid);
    const auto creation = bp::process_creation_stamp(process.value);
    std::array<bp::CaptureMailboxReader, 3> mailboxes;
    auto &mailbox = mailboxes[0];
    const unsigned capture_count = three_frames ? 3U : 1U;
    require(creation && mailbox.open(pid, creation) && mailbox.read().state == bp::MailboxState::idle,
            "open idle v0.8.1 bridge capture mailbox read-only before debug attach");
    for (unsigned i = 1; i < capture_count; ++i)
      require(mailboxes[i].open(pid, creation, i) && mailboxes[i].read().state == bp::MailboxState::idle,
              "open all three immutable capture slots before debug attach");
    bp::BarrierJournalReader journal;
    require(journal.open(pid, creation) && journal.read().valid && journal.read().state == 1,
            "open idle v0.8.1 barrier journal read-only before debug attach");
    Output output(argv[2]);
    output.write("{\"event\":\"start\",\"version\":\"0.7.1\",\"pid\":" + std::to_string(pid) +
        ",\"host_sha256\":\"" + kHostHash + "\",\"host_base\":\"" + hex(base) +
        "\",\"code_gates\":" + std::to_string(std::size(ec::kCodeGates)) +
        ",\"mailbox_schema\":1,\"mailbox_producer\":1500,\"mailbox_read_only\":true,\"journal_schema\":2,\"journal_read_only\":true,\"process_creation\":" + std::to_string(creation) +
        ",\"remote_memory_writes\":0,\"remote_calls\":0,\"breakpoints_installed\":0,\"repair_depth_once\":" +
        (once ? "true" : "false") + ",\"repair_depth_three_frames\":" + (three_frames ? "true" : "false") +
        ",\"frame_window_ms\":" + (three_frames ? "500" : "0") +
        ",\"max_context_corrections\":" + (three_frames ? "3" : repair ? "1" : "0") + "}");
    require(SetConsoleCtrlHandler(control, TRUE), "install cancellation handler");
    ec::DepthRetry retry(three_frames ? ec::DepthRetryMode::three_frames : ec::DepthRetryMode::once);
    std::array<bool, 3> mailbox_saved{};
    auto save_all = [&](const char *phase) {
      std::array<bp::MailboxRead, 3> captures{};
      for (unsigned i = 0; i < capture_count; ++i) {
        save_mailbox(mailboxes[i], output, mailbox_saved[i], phase, i);
        captures[i] = mailboxes[i].read();
      }
      return captures;
    };
    auto result = watch(pid, process.value, static_cast<DWORD>(seconds * 1000U),
      [&](const DEBUG_EVENT &event) {
        bool channels_ready = true;
        std::uint64_t capture_deadline = std::numeric_limits<std::uint64_t>::max();
        if (matches_exception(event.u.Exception.ExceptionRecord, base)) {
          // Write and flush the producer's result while the debug event still
          // holds the target stopped. Never continue the next fault first.
          const auto captures = save_all("matching-exception-before-continue");
          const auto trace = save_journal(journal, output, "matching-exception-before-continue", true);
          for (unsigned i = 0; three_frames && i < retry.attempts() && i < 3; ++i) {
            const auto review = bp::review_checkpoint(captures[i], trace, i);
            output.write("{\"event\":\"checkpoint-review\",\"index\":" + std::to_string(i) +
                ",\"accepted\":" + (review.accepted ? "true" : "false") + ",\"reason\":\"" + std::string(review.reason) +
                "\",\"epoch\":" + std::to_string(review.epoch) + ",\"selected_calls\":" + std::to_string(review.selected_calls) +
                ",\"unrelated_overlaps\":" + std::to_string(review.unrelated_overlaps) +
                ",\"query_overlaps\":" + std::to_string(review.query_overlaps) +
                ",\"declared_state_mask\":" + std::to_string(review.declared_state_mask) +
                ",\"expected_state_mask\":" + std::to_string(review.expected_state_mask) +
                ",\"unknown_state_mask\":" + std::to_string(review.unknown_state_mask) + ",\"target_stopped\":true,\"gpu_completion_verified\":false}");
          }
          capture_deadline = trace.deadline;
          channels_ready = three_frames ? ec::retry_sequence_ready(retry.attempts(), captures, trace, GetTickCount64()) :
              ec::retry_channels_ready(retry.attempts(), captures[0].state, trace, GetTickCount64());
        }
        return target_exception(event, process.value, base, output, repair ? &retry : nullptr, channels_ready, capture_deadline);
      },
      [&] {
        const auto captures = save_all("armed");
        for (unsigned i = 0; i < capture_count; ++i)
          require(captures[i].state == bp::MailboxState::idle, "mailbox changed before arming");
        const auto trace = save_journal(journal, output, "armed", false);
        require(trace.valid && trace.state == 1 && trace.completed == 0 && trace.pending == 0 && !trace.flags, "journal changed before arming");
        output.write("{\"event\":\"armed\",\"kill_on_debugger_exit\":false}");
        std::cout << "ARMED PID " << pid << (three_frames ? ": THREE consecutive-frame corrections maximum within 500 ms; " :
            repair ? ": ONE depth argument correction; " : ": capture only; ")
                  << seconds << " second limit.\n" << std::flush;
      });
    save_all("finished");
    save_journal(journal, output, "finished", false);
    output.write("{\"event\":\"finished\",\"captured\":" + std::string(result.captured ? "true" : "false") +
        ",\"detached\":" + (result.detached ? "true" : "false") +
        ",\"exited\":" + (result.exited ? "true" : "false") +
        ",\"events\":" + std::to_string(result.events) + ",\"exceptions\":" + std::to_string(result.exceptions) +
        ",\"context_attempts\":" + std::to_string(retry.attempts()) + ",\"instruction_retries\":" + std::to_string(result.retries) +
        ",\"bridge_capture_saved\":" + (mailbox_saved[0] ? "true" : "false") + "}");
    std::cout << "Finished: captured=" << result.captured << " detached=" << result.detached << "\n";
    return result.captured ? 0 : 3;
  } catch (const std::exception &e) {
    std::cerr << "Capture stopped: " << e.what() << '\n';
    return 1;
  }
}

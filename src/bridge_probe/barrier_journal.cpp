#include "barrier_journal.hpp"
#include "capture_mailbox.hpp"
#include <atomic>
#include <algorithm>
#include <new>
#include <sstream>

namespace mhw::bridge_probe {
namespace {
constexpr std::uint64_t magic = 0x314C4E4A4457484DULL;
struct Slot { alignas(4) unsigned state{}; unsigned checksum{}; BarrierCall call{}; };
struct Checkpoint {
  alignas(4) unsigned ready{}, unused{};
  std::uint64_t command{}, tick{}, order{};
  unsigned reserved{}, prefix{};
};
struct Terminal { alignas(4) unsigned ready{}, checksum{}; JournalTermination value{}; };
struct Journal {
  std::uint64_t magic{};
  unsigned schema{}, producer{}, bytes{}, pid{};
  std::uint64_t creation{};
  alignas(4) unsigned state{}, flags{}, checkpoint_ready{}, reserved{};
  std::uint64_t deadline{}, command{}, tick{}, order{};
  unsigned checkpoint_reserved{}, checkpoint_prefix{};
  std::array<Slot, kBarrierCallCapacity> slots{};
  std::array<D3D12_RESOURCE_BARRIER, kBarrierDescriptionCapacity> barriers{};
  std::array<std::uintptr_t, kSubmittedListCapacity> lists{};
  std::array<Checkpoint, 2> extra{};
  Terminal terminal{};
};
static_assert(sizeof(unsigned) == 4 && sizeof(std::uintptr_t) == 8 && sizeof(D3D12_RESOURCE_BARRIER) == 32);
static_assert(sizeof(BarrierCall) == 88 && offsetof(Journal, state) == 32 && offsetof(Journal, slots) == 88);
static_assert(offsetof(Journal, terminal) == 753832 && sizeof(Journal) == 753944);
static_assert(std::atomic_ref<unsigned>::is_always_lock_free);
auto atomic(const unsigned &v) noexcept { return std::atomic_ref<unsigned>(const_cast<unsigned &>(v)); }
bool header(const Journal &j, DWORD pid, std::uint64_t creation) noexcept {
  return j.magic == magic && j.schema == 2 && j.producer == kBarrierJournalProducer &&
      j.bytes == sizeof(Journal) && j.pid == pid && j.creation == creation && j.reserved == 0;
}
void hash_bytes(unsigned &h, const void *p, std::size_t n) noexcept {
  const auto bytes = static_cast<const unsigned char *>(p);
  for (std::size_t i = 0; i < n; ++i) { h ^= bytes[i]; h *= 16777619U; }
}
bool ranges(const BarrierCall &c) noexcept {
  if (static_cast<unsigned>(c.kind) > static_cast<unsigned>(BarrierCallKind::fence_cpu_signal)) return false;
  if (c.kind == BarrierCallKind::barrier)
    return c.count <= kBarrierBatchCapacity && c.first <= kBarrierDescriptionCapacity && c.count <= kBarrierDescriptionCapacity - c.first;
  if (c.kind == BarrierCallKind::execute)
    return c.count <= kSubmissionBatchCapacity && c.first <= kSubmittedListCapacity && c.count <= kSubmittedListCapacity - c.first;
  return c.count == 0;
}
unsigned checksum(const Journal &j, const BarrierCall &c) noexcept {
  unsigned h = 2166136261U;
  hash_bytes(h, &c, sizeof(c));
  if (c.kind == BarrierCallKind::barrier) hash_bytes(h, j.barriers.data() + c.first, c.count * sizeof(D3D12_RESOURCE_BARRIER));
  if (c.kind == BarrierCallKind::execute) hash_bytes(h, j.lists.data() + c.first, c.count * sizeof(std::uintptr_t));
  return h;
}
} // namespace
std::wstring barrier_journal_name(DWORD pid, std::uint64_t creation) {
  return L"Local\\MhwDlssBarrierJournal-v2-" + std::to_wstring(pid) + L"-" + std::to_wstring(creation);
}
BarrierJournalWriter::~BarrierJournalWriter() { if (view_) UnmapViewOfFile(view_); if (mapping_) CloseHandle(mapping_); }
bool BarrierJournalWriter::create() noexcept {
  if (view_ || mapping_) return false;
  try {
    const auto pid = GetCurrentProcessId(); const auto creation = process_creation_stamp(GetCurrentProcess());
    if (!creation) return false;
    const auto name = barrier_journal_name(pid, creation);
    auto map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Journal), name.c_str());
    const auto error = GetLastError();
    if (!map) return false;
    if (error == ERROR_ALREADY_EXISTS) { CloseHandle(map); return false; }
    auto view = MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, sizeof(Journal));
    if (!view) { CloseHandle(map); return false; }
    auto &j = *new(view) Journal{};
    j.magic = magic; j.schema = 2; j.producer = kBarrierJournalProducer; j.bytes = sizeof(Journal);
    j.pid = pid; j.creation = creation;
    atomic(j.state).store(1, std::memory_order_release);
    mapping_ = map; view_ = view; return true;
  } catch (...) { return false; }
}
void BarrierJournalWriter::start(std::uint64_t deadline) noexcept {
  if (!view_) return;
  auto &j = *static_cast<Journal *>(view_);
  if (atomic(j.state).load(std::memory_order_acquire) != 1) return;
  j.deadline = deadline; atomic(j.state).store(2, std::memory_order_release);
}
void BarrierJournalWriter::stop(unsigned flags) noexcept {
  if (!view_) return;
  auto &j = *static_cast<Journal *>(view_);
  atomic(j.flags).fetch_or(flags, std::memory_order_release);
  atomic(j.state).store(3, std::memory_order_release);
}
void BarrierJournalWriter::reserve(unsigned index) noexcept {
  if (!view_ || index >= kBarrierCallCapacity) return;
  auto &s = static_cast<Journal *>(view_)->slots[index];
  unsigned empty{};
  if (!atomic(s.state).compare_exchange_strong(empty, 1, std::memory_order_acq_rel)) stop(journal_fault);
}
void BarrierJournalWriter::terminate(const JournalTermination &value) noexcept {
  if (!view_) return;
  auto &j = *static_cast<Journal *>(view_);
  unsigned empty{};
  if (!atomic(j.terminal.ready).compare_exchange_strong(empty, 1, std::memory_order_acq_rel)) return;
  j.terminal.value = value;
  unsigned h = 2166136261U;
  hash_bytes(h, &j.terminal.value, sizeof(value));
  j.terminal.checksum = h;
  atomic(j.terminal.ready).store(2, std::memory_order_release);
}
void BarrierJournalWriter::publish(unsigned index, const BarrierCall &c,
    std::span<const D3D12_RESOURCE_BARRIER> barriers, std::span<const std::uintptr_t> lists) noexcept {
  if (!view_ || index >= kBarrierCallCapacity) return;
  auto &j = *static_cast<Journal *>(view_); auto &s = j.slots[index];
  // Only the owner of this reserved slot may publish; no reuse or overwriting.
  if (atomic(s.state).load(std::memory_order_acquire) != 1) { stop(journal_fault); return; }
  if (!ranges(c) || c.sequence != index + 1 || !c.entry_order || c.return_order <= c.entry_order ||
      barriers.size() != (c.kind == BarrierCallKind::barrier ? c.count : 0) ||
      lists.size() != (c.kind == BarrierCallKind::execute ? c.count : 0)) {
    atomic(s.state).store(3, std::memory_order_release); stop(journal_fault); return;
  }
  s.call = c;
  if (!barriers.empty()) std::copy(barriers.begin(), barriers.end(), j.barriers.begin() + c.first);
  if (!lists.empty()) std::copy(lists.begin(), lists.end(), j.lists.begin() + c.first);
  s.checksum = checksum(j, s.call);
  atomic(s.state).store(2, std::memory_order_release);
}
void BarrierJournalWriter::checkpoint(std::uintptr_t command, std::uint64_t tick, std::uint64_t order,
                                     unsigned reserved, unsigned prefix, unsigned index) noexcept {
  if (!view_ || index >= 3) return;
  auto &j = *static_cast<Journal *>(view_); unsigned empty{};
  if (index) {
    auto &c = j.extra[index - 1];
    if (!atomic(c.ready).compare_exchange_strong(empty, 1, std::memory_order_acq_rel)) return;
    c.command = command; c.tick = tick; c.order = order; c.reserved = reserved; c.prefix = prefix;
    atomic(c.ready).store(2, std::memory_order_release); return;
  }
  if (!atomic(j.checkpoint_ready).compare_exchange_strong(empty, 1, std::memory_order_acq_rel)) return;
  j.command = command; j.tick = tick; j.order = order;
  j.checkpoint_reserved = reserved; j.checkpoint_prefix = prefix;
  atomic(j.checkpoint_ready).store(2, std::memory_order_release);
}
BarrierJournalReader::~BarrierJournalReader() { if (view_) UnmapViewOfFile(view_); if (mapping_) CloseHandle(mapping_); }
bool BarrierJournalReader::open(DWORD pid, std::uint64_t creation) {
  if (view_ || mapping_ || !pid || !creation) return false;
  const auto name = barrier_journal_name(pid, creation);
  auto map = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
  if (!map) return false;
  auto view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, sizeof(Journal));
  if (!view) { CloseHandle(map); return false; }
  const auto &j = *static_cast<const Journal *>(view);
  const auto s = atomic(j.state).load(std::memory_order_acquire);
  if (s < 1 || s > 3 || !header(j, pid, creation)) { UnmapViewOfFile(view); CloseHandle(map); return false; }
  mapping_ = map; view_ = view; pid_ = pid; creation_ = creation; return true;
}
JournalRead BarrierJournalReader::read() const {
  JournalRead r{};
  if (!view_) return r;
  const auto &j = *static_cast<const Journal *>(view_);
  r.state = atomic(j.state).load(std::memory_order_acquire);
  if (r.state < 1 || r.state > 3 || !header(j, pid_, creation_)) return r;
  r.flags = atomic(j.flags).load(std::memory_order_acquire);
  const auto checkpoint = atomic(j.checkpoint_ready).load(std::memory_order_acquire);
  if (checkpoint > 2 || r.flags > 15) return r;
  r.checkpoint = checkpoint;
  std::ostringstream out;
  out << "{\"schema\":2,\"producer\":" << kBarrierJournalProducer << ",\"state\":" << r.state
      << ",\"flags\":" << r.flags << ",\"checkpoint_state\":" << checkpoint;
  if (r.state >= 2) { r.deadline = j.deadline; out << ",\"deadline\":" << r.deadline; }
  if (checkpoint == 2) {
    if (j.checkpoint_reserved > kBarrierCallCapacity || j.checkpoint_prefix > j.checkpoint_reserved) return r;
    r.checkpoint_reserved = j.checkpoint_reserved; r.checkpoint_prefix = j.checkpoint_prefix;
    r.evaluate_command = j.command; r.evaluate_order = j.order;
    out << ",\"evaluate_command\":" << j.command << ",\"evaluate_tick\":" << j.tick << ",\"evaluate_order\":" << j.order
        << ",\"checkpoint_reserved\":" << j.checkpoint_reserved << ",\"checkpoint_prefix\":" << j.checkpoint_prefix;
  }
  r.checkpoints[0] = {checkpoint, r.checkpoint_reserved, r.checkpoint_prefix,
      r.evaluate_command, checkpoint == 2 ? j.tick : 0, r.evaluate_order};
  out << ",\"checkpoints\":[";
  for (unsigned i = 0; i < 3; ++i) {
    auto &c = r.checkpoints[i];
    if (i) {
      const auto &v = j.extra[i - 1]; c.state = atomic(v.ready).load(std::memory_order_acquire);
      if (c.state > 2) return r;
      if (c.state == 2) c = {2, v.reserved, v.prefix, v.command, v.tick, v.order};
    }
    if (c.reserved > kBarrierCallCapacity || c.prefix > c.reserved) return r;
    if (i) out << ',';
    out << "{\"index\":" << i << ",\"state\":" << c.state << ",\"command\":" << c.command
        << ",\"tick\":" << c.tick << ",\"order\":" << c.order << ",\"reserved\":" << c.reserved << ",\"prefix\":" << c.prefix << '}';
  }
  out << "],\"calls\":[";
  // Scan every slot. A later call may finish before an earlier one; holes are
  // retained as pending/failed counts, never relabeled as continuous coverage.
  for (unsigned i = 0; i < j.slots.size(); ++i) {
    const auto &s = j.slots[i]; const auto state = atomic(s.state).load(std::memory_order_acquire);
    if (!state) continue;
    if (state == 1) { ++r.pending; continue; }
    if (state == 3) { ++r.failed; continue; }
    if (state != 2) return r;
    const auto &c = s.call;
    if (!ranges(c) || c.sequence != i + 1 || !c.entry_order || c.return_order <= c.entry_order || checksum(j, c) != s.checksum) return r;
    r.records.push_back(c);
    if (r.completed++) out << ',';
    out << "{\"call\":" << c.sequence << ",\"kind\":" << static_cast<unsigned>(c.kind) << ",\"command\":" << c.command
        << ",\"caller\":" << c.caller << ",\"tick\":" << c.tick << ",\"thread\":" << c.thread
        << ",\"entry\":" << c.entry_order << ",\"returned\":" << c.return_order << ",\"result\":" << static_cast<unsigned>(c.result)
        << ",\"queue\":" << c.queue << ",\"fence\":" << c.fence << ",\"value\":" << c.fence_value;
    if (c.kind == BarrierCallKind::execute) {
      r.lists = (std::max)(r.lists, c.first + c.count);
      out << ",\"lists\":[";
      for (unsigned n = 0; n < c.count; ++n) { if (n) out << ','; out << j.lists[c.first + n]; }
      out << ']';
    }
    if (c.kind == BarrierCallKind::barrier) {
      r.descriptions = (std::max)(r.descriptions, c.first + c.count);
      r.barrier_records.resize(r.descriptions);
      std::copy_n(j.barriers.data() + c.first, c.count, r.barrier_records.data() + c.first);
      out << ",\"barriers\":[";
      for (unsigned n = 0; n < c.count; ++n) {
        const auto &b = j.barriers[c.first + n];
        if (n) out << ',';
        out << "{\"index\":" << n << ",\"type\":" << b.Type << ",\"flags\":" << b.Flags;
        if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
          out << ",\"resource\":" << reinterpret_cast<std::uintptr_t>(b.Transition.pResource) << ",\"subresource\":" << b.Transition.Subresource
              << ",\"before\":" << b.Transition.StateBefore << ",\"after\":" << b.Transition.StateAfter;
        else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
          out << ",\"resource\":" << reinterpret_cast<std::uintptr_t>(b.UAV.pResource);
        else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
          out << ",\"resource_before\":" << reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)
              << ",\"resource_after\":" << reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter);
        else return r;
        out << '}';
      }
      out << ']';
    }
    out << '}';
  }
  out << "]";
  const auto terminal_ready = atomic(j.terminal.ready).load(std::memory_order_acquire);
  if (terminal_ready > 2) return r;
  r.drained = terminal_ready == 2;
  out << ",\"drained\":" << (r.drained ? "true" : "false");
  if (r.drained) {
    unsigned h = 2166136261U; hash_bytes(h, &j.terminal.value, sizeof(JournalTermination));
    if (h != j.terminal.checksum) return r;
    r.termination = j.terminal.value;
    const auto &t = r.termination; const auto &c = t.completion;
    if (t.frontier > kBarrierCallCapacity || t.unused || t.flags != r.flags || r.pending ||
        static_cast<unsigned>(t.reason) > 7 || !t.order) return r;
    out << ",\"termination\":{\"reason\":" << static_cast<unsigned>(t.reason)
        << ",\"flags\":" << t.flags << ",\"frontier\":" << t.frontier << ",\"order\":" << t.order
        << ",\"status\":" << static_cast<unsigned>(c.status) << ",\"issue\":" << static_cast<unsigned>(c.issue)
        << ",\"reset\":" << c.reset << ",\"close\":" << c.close << ",\"submission\":" << c.submission
        << ",\"signal\":" << c.signal << ",\"completed\":" << c.completed << ",\"command\":" << c.command
        << ",\"queue\":" << c.queue << ",\"fence\":" << c.fence << ",\"marker\":" << c.marker
        << ",\"value\":" << c.value << ",\"completed_order\":" << c.completed_order << '}';
  }
  out << ",\"completed\":" << r.completed << ",\"pending\":" << r.pending << ",\"failed\":" << r.failed
      << ",\"gpu_completion_qualified\":false,\"end_of_frame_proven\":false}";
  r.valid = true; r.json = out.str(); return r;
}
} // namespace mhw::bridge_probe

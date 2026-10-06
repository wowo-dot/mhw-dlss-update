#include "capture_mailbox.hpp"
#include "barrier_journal.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
namespace b = mhw::bridge_probe;
void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
struct Handle {
  HANDLE value{};
  explicit Handle(HANDLE h = nullptr) : value(h) {}
  ~Handle() { if (value) CloseHandle(value); }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
};
struct View {
  void *value{};
  explicit View(void *p) : value(p) { check(p != nullptr, "map test view"); }
  ~View() { UnmapViewOfFile(value); }
};
struct Fixture {
  b::CaptureMetadata metadata{69, 123456, UINT64_MAX, 0x10000B12D, 42, 0xBAD00012, true, true, true, true, true};
  b::EvaluateContractSnapshot contract{};
  b::ResourceInspection inspection{};
  b::BarrierCapture barriers{};
  b::PresentationCreation presentation{};
  Fixture() {
    contract.summary.complete = true; contract.summary.queries_attempted = b::kEvaluateQueryCount;
    for (unsigned i = 0; i < contract.resources.size(); ++i) {
      auto &r = contract.resources[i]; r.key = static_cast<b::EvaluateResourceKey>(i);
      r.value = i == 2 ? 0 : UINT64_MAX - i; r.value_is_null = !r.value;
      r.query_attempted = r.callback_completed = r.present = true; r.getter_result = 1;
      inspection.resources[i].key = r.key;
      inspection.resources[i].description.Width = UINT64_MAX;
      inspection.resources[i].description.Height = UINT_MAX;
    }
    for (unsigned i = 0; i < contract.floats.size(); ++i) {
      auto &r = contract.floats[i]; r.key = static_cast<b::EvaluateFloatKey>(i);
      r.value = std::bit_cast<float>(0x7FC01234U); // Preserve NaN bits; never emit invalid JSON NaN.
    }
    for (unsigned i = 0; i < contract.signed_values.size(); ++i) {
      auto &r = contract.signed_values[i]; r.key = static_cast<b::EvaluateSignedKey>(i); r.value = INT32_MIN;
    }
    const b::BarrierMatch match{UINT64_MAX, UINT64_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX,
        UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
    for (unsigned i = 0; i < barriers.resources.size(); ++i) {
      auto &r = barriers.resources[i]; r.key = static_cast<b::EvaluateResourceKey>(i);
      r.retained = static_cast<unsigned>(r.recent.size()); r.recent.fill(match);
    }
    auto &t = barriers.depth_timeline;
    t.retained = static_cast<unsigned>(t.recent.size()); t.recent.fill(match);
    t.controls_retained = static_cast<unsigned>(t.control_events.size());
    t.queues_retained = static_cast<unsigned>(t.queue_events.size());
    t.list_count = static_cast<unsigned>(t.submitted_lists.size()); t.submitted_lists.fill(UINT64_MAX);
    b::BarrierCall c{};
    c.command = c.caller = c.queue = c.fence = UINT64_MAX;
    c.tick = c.fence_value = c.entry_order = c.return_order = UINT64_MAX;
    c.thread = c.sequence = UINT_MAX; c.result = E_FAIL;
    c.kind = b::BarrierCallKind::close; t.control_events.fill(c);
    c.kind = b::BarrierCallKind::signal; t.queue_events.fill(c);
    t.queue_events[0].kind = b::BarrierCallKind::execute; t.queue_events[0].first = 0; t.queue_events[0].count = 256;
  }
  std::size_t format(std::span<char> dst) const {
    return b::format_capture_json(dst, metadata, &contract, &inspection, &barriers, &presentation);
  }
  bool publish(b::CaptureMailboxWriter &writer) const {
    return writer.publish(metadata, &contract, &inspection, &barriers, &presentation);
  }
};
std::string sample() {
  Fixture f;
  std::vector<char> buffer(b::kCaptureJsonCapacity);
  const auto size = f.format(buffer);
  check(size > 40000 && size < buffer.size(), "maximum retained payload fits bounded JSON capacity");
  std::string result(buffer.data(), size);
  check(result.starts_with("{\"schema\":1,\"producer\":1500") && result.ends_with("}"), "JSON envelope");
  check(result.find("18446744073709551615") != std::string::npos && result.find("-2147483648") != std::string::npos,
        "64-bit pointer labels and signed values not narrowed");
  check(result.find("\"bits\":2143294004") != std::string::npos, "float payload is exact bits");
  std::array<char, 64> tiny{}; tiny.fill('#');
  check(f.format(std::span(tiny).first(8)) == 0, "tiny destination rejects partial JSON");
  check(std::all_of(tiny.begin() + 8, tiny.end(), [](char c) { return c == '#'; }), "formatter does not write past span");
  check(f.format({}) == 0, "empty destination rejected");
  auto &t = f.barriers.depth_timeline;
  t.retained = 17; check(f.format(buffer) == 0, "invalid depth match count rejected before reading"); t.retained = 16;
  t.controls_retained = 65; check(f.format(buffer) == 0, "invalid control count rejected"); t.controls_retained = 64;
  t.queues_retained = 65; check(f.format(buffer) == 0, "invalid queue count rejected"); t.queues_retained = 64;
  t.list_count = 257; check(f.format(buffer) == 0, "invalid submitted list count rejected"); t.list_count = 256;
  t.queue_events[0].first = 1; check(f.format(buffer) == 0, "invalid submission range rejected"); t.queue_events[0].first = 0;
  f.barriers.resources[0].retained = 5; check(f.format(buffer) == 0, "invalid per-resource match count rejected");
  return result;
}
void local_transport() {
  Fixture f;
  b::CaptureMailboxWriter writer;
  check(writer.create(), "create owned mailbox");
  b::CaptureMailboxWriter duplicate;
  check(!duplicate.create(), "existing mapping is not overwritten");
  const auto pid = GetCurrentProcessId(); const auto creation = b::process_creation_stamp(GetCurrentProcess());
  b::CaptureMailboxReader missing;
  check(!missing.open(pid, creation + 1), "process generation mismatch rejected");
  b::CaptureMailboxReader reader;
  check(reader.open(pid, creation), "open read-only mailbox");
  check(reader.read().state == b::MailboxState::idle && reader.read().json.empty(), "idle is not a completed capture");
  check(!f.publish(writer), "publish without Evaluate entry rejected");
  check(writer.enter() && !writer.enter(), "exactly one Evaluate entry");
  check(reader.read().state == b::MailboxState::entered && reader.read().json.empty(), "entry without completion has no payload");
  check(f.publish(writer), "publish full bounded result");
  const auto saved = reader.read();
  check(saved.state == b::MailboxState::published && saved.json == sample(), "read-only view recovers exact payload");
  check(!f.publish(writer) && !writer.enter() && reader.read().json == saved.json, "completed record is immutable");

  // Mutations below target only this owned test mapping, never another process.
  const auto name = b::capture_mailbox_name(pid, creation);
  Handle map(OpenFileMappingW(FILE_MAP_WRITE, FALSE, name.c_str())); check(map.value != nullptr, "open test corruption view");
  View view(MapViewOfFile(map.value, FILE_MAP_WRITE, 0, 0, 0));
  auto bytes = static_cast<std::byte *>(view.value);
  auto word = [&](unsigned offset) -> std::uint32_t & { return *reinterpret_cast<std::uint32_t *>(bytes + offset); };
  bytes[48 + 30] ^= std::byte{1};
  check(reader.read().state == b::MailboxState::invalid, "corrupt payload checksum rejected"); bytes[48 + 30] ^= std::byte{1};
  const auto length = word(36); word(36) = static_cast<std::uint32_t>(b::kCaptureJsonCapacity + 1);
  check(reader.read().state == b::MailboxState::invalid, "oversized payload rejected without copy"); word(36) = length;
  word(12) = 708; check(reader.read().state == b::MailboxState::invalid, "wrong producer rejected"); word(12) = b::kCaptureMailboxProducer;
  word(20) = pid + 1; check(reader.read().state == b::MailboxState::invalid, "wrong owner rejected"); word(20) = pid;
  std::atomic_ref(word(32)).store(static_cast<unsigned>(b::MailboxState::writing));
  check(reader.read().state == b::MailboxState::writing && reader.read().json.empty(), "partial publication never treated as complete");
  std::atomic_ref(word(32)).store(static_cast<unsigned>(b::MailboxState::failed));
  check(reader.read().state == b::MailboxState::failed && reader.read().json.empty(), "producer rejection reported without payload");
}
void three_slots() {
  std::array<b::CaptureMailboxWriter, 3> writers;
  std::array<b::CaptureMailboxReader, 3> readers;
  std::array<std::string, 3> saved;
  for (unsigned i = 0; i < 3; ++i) {
    check(writers[i].create(i) && readers[i].open(GetCurrentProcessId(), b::process_creation_stamp(GetCurrentProcess()), i), "three independent slots");
    Fixture f; f.metadata.evaluate_index = i; f.metadata.sequence += i;
    f.inspection.complete = f.barriers.coverage_complete = true;
    f.inspection.command_identity_verified = true;
    f.barriers.installed = f.barriers.armed = f.barriers.finished = f.barriers.methods_match = f.barriers.contract_valid = true;
    f.barriers.command = 123; f.barriers.evaluate_thread = f.metadata.thread;
    check(writers[i].enter() && f.publish(writers[i]), "publish independently indexed record");
    const auto result = readers[i].read(); saved[i] = result.json;
    check(result.qualified && result.state == b::MailboxState::published, "qualified capture survives immutable transport");
    check(result.inputs_verified && result.binding.index == i && result.binding.command == 123 &&
          result.binding.resources[3] == f.contract.resources[3].value, "typed replay binding matches capture");
    for (unsigned j = 0; j <= i; ++j) check(readers[j].read().json == saved[j], "later slots cannot overwrite earlier inputs");
    check(!f.publish(writers[i]), "no slot can be overwritten");
    const auto name = b::capture_mailbox_name(GetCurrentProcessId(), b::process_creation_stamp(GetCurrentProcess()), i);
    Handle map(OpenFileMappingW(FILE_MAP_WRITE, FALSE, name.c_str()));
    View view(MapViewOfFile(map.value, FILE_MAP_WRITE, 0, 0, 0));
    auto bytes = static_cast<std::byte *>(view.value);
    for (const auto offset : {48 + b::kCaptureJsonCapacity + 4, 56 + b::kCaptureJsonCapacity,
                              56 + b::kCaptureJsonCapacity + 8}) {
      bytes[offset] ^= std::byte{1};
      check(readers[i].read().state == b::MailboxState::invalid, "qualification, command and resource labels covered by checksum");
      bytes[offset] ^= std::byte{1};
    }
    check(readers[i].read().inputs_verified, "restored owned binding is valid");
  }
  b::CaptureMailboxWriter extra; check(!extra.create(3), "fourth capture slot rejected");
}
void journal_sample(b::BarrierJournalWriter &writer) {
  writer.start(GetTickCount64() + 2000);
  b::BarrierCall call{}; call.kind = b::BarrierCallKind::reset; call.sequence = 1;
  call.command = 0xFFFFFFFF12345678ULL; call.entry_order = 1; call.return_order = 2; call.result = S_OK;
  writer.reserve(0); writer.publish(0, call, {}, {});
  writer.checkpoint(call.command, 12345, 3, 1, 1);
  call.kind = b::BarrierCallKind::execute; call.sequence = 2; call.entry_order = 4; call.return_order = 5;
  call.count = 1; call.queue = 0xFFFFFFFFFFFFFFFFULL;
  const std::array<std::uintptr_t, 1> lists{call.command};
  writer.reserve(1); writer.publish(1, call, {}, lists);
}
void journal_transport() {
  b::BarrierJournalWriter writer, duplicate;
  check(writer.create() && !duplicate.create(), "journal duplicate mapping rejected");
  const auto pid = GetCurrentProcessId(); const auto creation = b::process_creation_stamp(GetCurrentProcess());
  b::BarrierJournalReader reader, wrong;
  check(!wrong.open(pid, creation + 1) && reader.open(pid, creation), "journal process identity");
  check(reader.read().valid && reader.read().state == 1 && reader.read().completed == 0, "journal initially idle");
  journal_sample(writer);
  auto saved = reader.read();
  check(saved.valid && saved.completed == 2 && !saved.pending && !saved.flags &&
        saved.json.find("18446744073709551615") != std::string::npos, "journal exact labels and post-checkpoint call");
  writer.reserve(2);
  check(reader.read().valid && reader.read().pending == 1 && reader.read().completed == 2, "unfinished slot not consumed");
  b::BarrierCall later{}; later.kind = b::BarrierCallKind::close; later.sequence = 4;
  later.entry_order = 8; later.return_order = 9; later.result = S_OK;
  writer.reserve(3); writer.publish(3, later, {}, {});
  check(reader.read().valid && reader.read().pending == 1 && reader.read().completed == 3,
        "later completed slot retained despite an earlier unfinished call");
  b::BarrierCall malformed{}; malformed.kind = b::BarrierCallKind::execute; malformed.sequence = 3;
  malformed.entry_order = 6; malformed.return_order = 7; malformed.count = b::kSubmissionBatchCapacity + 1;
  writer.publish(2, malformed, {}, {});
  check(reader.read().valid && reader.read().failed == 1 && reader.read().flags == b::journal_fault, "bad range fails without copy");
  const auto name = b::barrier_journal_name(pid, creation);
  Handle map(OpenFileMappingW(FILE_MAP_WRITE, FALSE, name.c_str())); check(map.value != nullptr, "owned journal mutation view");
  View view(MapViewOfFile(map.value, FILE_MAP_WRITE, 0, 0, 0));
  auto bytes = static_cast<std::byte *>(view.value);
  bytes[92] ^= std::byte{1};
  check(!reader.read().valid && reader.read().json.empty(), "corrupt slot checksum rejected");
  bytes[92] ^= std::byte{1};
  auto &producer = *reinterpret_cast<unsigned *>(bytes + 12);
  producer = 708; check(!reader.read().valid, "old journal producer rejected"); producer = b::kBarrierJournalProducer;
  writer.reserve(0); // Duplicate reservation cannot overwrite the first record.
  check(reader.read().valid && reader.read().completed == 3, "published slots never reused");
}
std::string journal_capacity() {
  b::BarrierJournalWriter writer;
  check(writer.create(), "capacity journal");
  writer.start(GetTickCount64() + 2000);
  std::array<D3D12_RESOURCE_BARRIER, b::kBarrierBatchCapacity> barriers{};
  for (auto &v : barriers) {
    v.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    v.Transition.pResource = reinterpret_cast<ID3D12Resource *>(0xFFFFFFFF12345678ULL);
    v.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    v.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    v.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  }
  std::array<std::uintptr_t, b::kSubmissionBatchCapacity> lists{}; lists.fill(0xFFFFFFFFFFFFFFFFULL);
  for (unsigned i = 0; i < b::kBarrierCallCapacity; ++i) {
    b::BarrierCall c{}; c.sequence = i + 1; c.result = S_OK;
    c.entry_order = 2ULL * i + 1; c.return_order = 2ULL * i + 2;
    c.kind = b::BarrierCallKind::close;
    if (i < 16) { c.kind = b::BarrierCallKind::barrier; c.first = i * b::kBarrierBatchCapacity; c.count = b::kBarrierBatchCapacity; }
    else if (i < 48) { c.kind = b::BarrierCallKind::execute; c.first = (i - 16) * b::kSubmissionBatchCapacity; c.count = b::kSubmissionBatchCapacity; }
    writer.reserve(i); writer.publish(i, c, i < 16 ? std::span(barriers) : std::span<const D3D12_RESOURCE_BARRIER>{},
                                      i >= 16 && i < 48 ? std::span(lists) : std::span<const std::uintptr_t>{});
  }
  writer.stop(b::journal_overflow);
  b::BarrierJournalReader reader;
  check(reader.open(GetCurrentProcessId(), b::process_creation_stamp(GetCurrentProcess())), "capacity reader");
  const auto saved = reader.read();
  check(saved.valid && saved.completed == b::kBarrierCallCapacity && !saved.pending &&
        saved.flags == b::journal_overflow && saved.state == 3, "full pools retained with explicit stop reason");
  return saved.json;
}
int child(HANDLE ready, HANDLE go, bool partial) {
  b::CaptureMailboxWriter writer;
  b::BarrierJournalWriter journal;
  check(writer.create() && journal.create(), "child create");
  check(SetEvent(ready), "signal child readiness");
  check(WaitForSingleObject(go, 5000) == WAIT_OBJECT_0, "child reader attached");
  check(writer.enter(), "child Evaluate entry");
  Fixture f;
  if (partial) f.barriers.depth_timeline.retained = 17;
  check(f.publish(writer) != partial, "child publication status");
  journal_sample(journal);
  if (partial) journal.reserve(2);
  // Own fixture process only: exit without destructors or any text-log writer.
  TerminateProcess(GetCurrentProcess(), 73);
  return 74;
}
void death_test(bool partial) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  Handle ready(CreateEventW(&sa, TRUE, FALSE, nullptr)), go(CreateEventW(&sa, TRUE, FALSE, nullptr));
  check(ready.value && go.value, "fixture events");
  wchar_t exe[32768]{}; check(GetModuleFileNameW(nullptr, exe, 32768) != 0, "fixture path");
  std::wstring command = L"\"" + std::wstring(exe) + L"\" --child " +
      std::to_wstring(reinterpret_cast<std::uintptr_t>(ready.value)) + L" " +
      std::to_wstring(reinterpret_cast<std::uintptr_t>(go.value)) + (partial ? L" 1" : L" 0");
  STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
  check(CreateProcessW(exe, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process), "launch owned fixture");
  Handle target(process.hProcess), thread(process.hThread);
  check(WaitForSingleObject(ready.value, 5000) == WAIT_OBJECT_0, "child ready");
  b::CaptureMailboxReader reader;
  check(reader.open(process.dwProcessId, b::process_creation_stamp(target.value)), "hold read-only child mapping before publication");
  b::BarrierJournalReader journal;
  check(journal.open(process.dwProcessId, b::process_creation_stamp(target.value)), "hold read-only child journal");
  check(reader.read().state == b::MailboxState::idle, "child initially idle");
  check(SetEvent(go.value), "let fixture publish and exit");
  check(WaitForSingleObject(target.value, 5000) == WAIT_OBJECT_0, "fixture process exited");
  DWORD code{}; check(GetExitCodeProcess(target.value, &code) && code == 73, "fixture abrupt exit confirmed");
  const auto capture = reader.read();
  if (partial) check(capture.state == b::MailboxState::failed && capture.json.empty(), "failed producer survives as failed, never complete");
  else check(capture.state == b::MailboxState::published && capture.json == sample(), "complete data survives producer termination without worker scheduling");
  const auto trace = journal.read();
  check(trace.valid && trace.completed == 2 && trace.pending == (partial ? 1U : 0U), "journal completed slots and pending hole survive abrupt exit");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc == 5 && std::wstring_view(argv[1]) == L"--child")
      return child(reinterpret_cast<HANDLE>(std::stoull(argv[2])), reinterpret_cast<HANDLE>(std::stoull(argv[3])), std::wstring_view(argv[4]) == L"1");
    const auto json = sample();
    local_transport(); three_slots(); journal_transport(); death_test(false); death_test(true);
    const auto full_journal = journal_capacity();
    if (argc == 3 && std::wstring_view(argv[1]) == L"--emit") {
      std::ofstream out(std::filesystem::path(argv[2]), std::ios::binary); out << json << '\n'; check(out.good(), "write owned fixture JSON");
      std::ofstream journal(std::filesystem::path(argv[2]).wstring() + L".journal.json", std::ios::binary);
      journal << full_journal << '\n'; check(journal.good(), "write full journal fixture JSON");
    }
    std::cout << "Mailbox bounds, immutable publication, read-only access, corruption rejection and abrupt producer-exit tests passed. Max fixture bytes=" << json.size() << '\n';
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

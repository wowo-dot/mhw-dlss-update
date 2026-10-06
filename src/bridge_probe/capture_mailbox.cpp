#include "capture_mailbox.hpp"
#include "jitter_sequence.hpp"
#include <atomic>
#include <bit>
#include <charconv>
#include <cstring>
#include <new>

namespace mhw::bridge_probe {
namespace {
struct Json {
  std::span<char> dst;
  std::size_t used{};
  bool ok{true};
  void raw(std::string_view v) noexcept {
    if (!ok || v.size() > dst.size() - used) { ok = false; return; }
    std::memcpy(dst.data() + used, v.data(), v.size()); used += v.size();
  }
  void quote(std::string_view v) noexcept { raw("\""); raw(v); raw("\""); }
  void key(std::string_view name) noexcept {
    if (used && dst[used - 1] != '{') raw(",");
    quote(name); raw(":");
  }
  template <typename T> void number(T v) noexcept {
    char text[32]{}; const auto result = std::to_chars(text, text + sizeof(text), v);
    if (result.ec != std::errc{}) { ok = false; return; }
    raw({text, static_cast<std::size_t>(result.ptr - text)});
  }
  void n(std::string_view name, std::uint64_t v) noexcept { key(name); number(v); }
  void s(std::string_view name, std::int64_t v) noexcept { key(name); number(v); }
  void b(std::string_view name, bool v) noexcept { key(name); raw(v ? "true" : "false"); }
  void object(std::string_view name) noexcept { key(name); raw("{"); }
  void array(std::string_view name) noexcept { key(name); raw("["); }
  void item() noexcept { if (used && dst[used - 1] != '[') raw(","); raw("{"); }
};
void match(Json &j, const BarrierMatch &m) noexcept {
  j.n("call", m.sequence); j.n("command", m.command); j.n("caller", m.caller);
  j.n("tick", m.tick); j.n("thread", m.thread); j.n("entry", m.entry_order); j.n("returned", m.return_order);
  j.n("index", m.index); j.n("batch", m.batch_size); j.n("type", m.type); j.n("flags", m.flags);
  j.n("subresource", m.subresource); j.n("before", m.before); j.n("after", m.after);
}
void call(Json &j, const BarrierCall &c) noexcept {
  j.n("call", c.sequence); j.n("kind", static_cast<unsigned>(c.kind)); j.n("command", c.command);
  j.n("caller", c.caller); j.n("tick", c.tick); j.n("thread", c.thread);
  j.n("entry", c.entry_order); j.n("returned", c.return_order); j.n("result", static_cast<std::uint32_t>(c.result));
  j.n("queue", c.queue); j.n("fence", c.fence); j.n("value", c.fence_value); j.n("first", c.first); j.n("count", c.count);
}
void frame(Json &j, const FramePacket &f) noexcept {
  j.n("schema", f.schema); j.n("stage_count", f.stage_count); j.n("rejection", f.rejection);
  j.b("queued", f.queued); j.b("consumer_bound", f.consumer_bound); j.b("evaluate_bound", f.evaluate_bound);
  j.b("publication_attempted", f.publication_attempted); j.b("publication_bound", f.publication_bound);
  j.b("source_frame_link_verified", f.publication_bound && f.evaluate_bound && !f.rejection);
  j.b("temporal_alignment_verified", false);
  j.n("command", f.command); j.n("evaluate_frame", f.evaluate_frame);
  j.b("shader_jitter_verified", false); j.b("jitter_units_verified", false);
  j.array("resources");
  for (unsigned i = 0; i < f.resources.size(); ++i) { if (i) j.raw(","); j.number(f.resources[i]); }
  j.raw("]"); j.object("producer");
  const auto &p = f.producer;
  j.b("valid", p.valid); j.n("reads", p.reads); j.n("checkpoint", p.checkpoint);
  j.n("renderer", p.renderer); j.n("table", p.table); j.n("view", p.view); j.n("recording_context", p.recording_context);
  j.n("frame", p.frame); j.n("frame_after", p.frame_after); j.n("current_index", p.current_index);
  j.n("epoch", p.epoch); j.n("bank", p.bank); j.n("async_mode", p.async_mode);
  j.n("next_index", p.next_index); j.n("selected_index", p.selected_index); j.n("sharpen", p.sharpen);
  j.n("reset_input", p.reset_input); j.n("reset_value_bits", p.reset_value_bits);
  j.n("jitter_mode", p.jitter_mode); j.n("jitter_parameter", p.jitter_parameter);
  j.array("jitter_bits"); j.number(p.jitter_bits[0]); j.raw(","); j.number(p.jitter_bits[1]); j.raw("]");
  j.array("rect"); for (unsigned i = 0; i < 4; ++i) { if (i) j.raw(","); j.number(p.rect[i]); } j.raw("]");
  const auto &c = p.projection; j.object("projection");
  j.b("attempted", c.attempted); j.b("valid", c.valid); j.b("matrix_relation", c.matrix_relation);
  j.n("rejection", static_cast<unsigned>(c.rejection)); j.n("vtable", c.vtable); j.n("vtable_rva", c.vtable_rva);
  j.b("shader_execution_verified", false); j.b("raster_jitter_verified", false);
  j.n("reads", c.reads); j.n("checkpoint", c.checkpoint); j.n("epoch", c.epoch);
  j.n("manager", c.manager); j.n("metadata_table", c.metadata_table); j.n("metadata", c.metadata);
  j.n("tagged_constants", c.tagged_constants); j.n("constants", c.constants);
  j.n("buffer_id", c.buffer_id); j.n("projection_id", c.projection_id); j.n("unjittered_id", c.unjittered_id); j.n("words", c.words);
  const auto bits = [&j](std::string_view name, const auto &values) {
    j.array(name); for (unsigned i = 0; i < values.size(); ++i) { if (i) j.raw(","); j.number(values[i]); } j.raw("]");
  };
  bits("jitter_bits", c.jitter_bits); bits("table20_bits", c.table20_bits); bits("viewport_bits", c.viewport_bits);
  bits("projection_bits", c.projection_bits); bits("unjittered_bits", c.unjittered_bits); j.raw("}}");
  const auto &q = f.publication; j.object("publication");
  j.b("valid", q.valid); j.n("reads", q.reads); j.n("checkpoint", q.checkpoint);
  j.n("renderer", q.renderer); j.n("recording_context", q.recording_context);
  j.n("entry", q.entry); j.n("entry_base", q.entry_base); j.n("index", q.index);
  j.n("count", q.count); j.n("capacity", q.capacity); j.n("key", q.key);
  j.n("descriptor", q.descriptor); j.n("payload", q.payload); j.n("wrapper", q.wrapper);
  j.n("frame", q.frame); j.n("frame_after", q.frame_after); j.n("epoch", q.epoch);
  j.n("bank", q.bank); j.n("async_mode", q.async_mode); j.n("reset_value_bits", q.reset_value_bits);
  j.array("owners"); for (unsigned i = 0; i < 4; ++i) { if (i) j.raw(","); j.number(q.owners[i]); } j.raw("]");
  j.array("rect_bits"); for (unsigned i = 0; i < 4; ++i) { if (i) j.raw(","); j.number(q.rect_bits[i]); } j.raw("]}");
  j.array("stages");
  for (unsigned i = 0; i < (std::min)(f.stage_count, 5U); ++i) {
    const auto &s = f.stages[i]; j.item(); j.n("stage", i); j.b("valid", s.valid); j.n("reads", s.reads);
    j.n("renderer", s.renderer); j.n("payload", s.payload); j.n("descriptor", s.descriptor); j.n("context", s.context);
    j.n("frame", s.frame); j.n("frame_after", s.frame_after); j.n("epoch", s.epoch); j.n("reset_value_bits", s.reset_value_bits);
    j.n("renderer_epoch", s.renderer_epoch); j.n("bank", s.bank); j.n("async_mode", s.async_mode);
    j.array("rect_bits"); for (unsigned n = 0; n < 4; ++n) { if (n) j.raw(","); j.number(s.rect_bits[n]); } j.raw("]}");
  }
  j.raw("]");
}
constexpr std::uint64_t magic = 0x315041434457484DULL;
struct alignas(8) Mailbox {
  std::uint64_t magic{};
  std::uint32_t schema{}, producer{}, bytes{}, pid{};
  std::uint64_t creation{};
  alignas(4) std::uint32_t state{}, length{}, checksum{}, reserved{};
  std::array<char, kCaptureJsonCapacity> json{};
  unsigned qualified{}, inputs_verified{};
  CaptureBinding binding{};
};
static_assert(std::atomic_ref<std::uint32_t>::is_always_lock_free);
static_assert(offsetof(Mailbox, state) == 32 && offsetof(Mailbox, json) == 48);
static_assert(sizeof(Mailbox) == 112 + kCaptureJsonCapacity);
static_assert(offsetof(Mailbox, binding) == 56 + kCaptureJsonCapacity);
// MSVC x64 lock-free acquire load is a read only operation, also on FILE_MAP_READ.
std::atomic_ref<std::uint32_t> state(const Mailbox &m) noexcept {
  return std::atomic_ref<std::uint32_t>(const_cast<std::uint32_t &>(m.state));
}
std::uint32_t checksum(std::string_view s) noexcept {
  std::uint32_t h = 2166136261U;
  for (const unsigned char c : s) { h ^= c; h *= 16777619U; }
  return h;
}
std::uint32_t payload_checksum(const Mailbox &m, std::string_view json) noexcept {
  auto h = checksum(json);
  const auto data = reinterpret_cast<const unsigned char *>(&m) + offsetof(Mailbox, qualified);
  for (std::size_t i = 0; i < 8 + sizeof(CaptureBinding); ++i) { h ^= data[i]; h *= 16777619U; }
  return h;
}
bool header(const Mailbox &m, DWORD pid, std::uint64_t creation, unsigned index) noexcept {
  return m.magic == magic && m.schema == kCaptureMailboxSchema && m.producer == kCaptureMailboxProducer &&
      m.bytes == sizeof(Mailbox) && m.pid == pid && m.creation == creation && m.reserved == index;
}
} // namespace
std::size_t format_jitter_sample_json(std::span<char> out, const JitterSample &s) noexcept {
  Json j{out}; j.raw("{"); j.n("schema", 2); j.n("producer", kCaptureMailboxProducer);
  j.n("index", s.index); j.n("phase", s.phase); j.n("tick", s.tick); j.n("thread", s.thread);
  j.b("valid", s.valid); j.b("duplicate", s.duplicate); j.b("gpu_execution_verified", false);
  const auto &g = s.gates; j.object("gates");
  j.b("attempted", g.attempted); j.b("valid", g.valid); j.b("predicate", g.predicate); j.n("reads", g.reads); j.n("rejection", g.rejection);
  j.n("layer", g.layer); j.n("vtable", g.vtable); j.n("vtable_rva", g.vtable_rva);
  j.n("wrapper_slot", g.wrapper_slot); j.n("wrapper", g.wrapper);
  j.n("layer_type", g.layer_type); j.n("layer_flags", g.layer_flags); j.n("view_flags", g.view_flags);
  j.n("blend_bits", g.blend_bits); j.n("enabled", g.enabled); j.n("use_dlss", g.use_dlss);
  j.array("reset_vector_bits"); j.number(g.reset_vector_bits[0]); j.raw(","); j.number(g.reset_vector_bits[1]); j.raw("]}");
  FramePacket packet{}; packet.producer = s.frame;
  j.object("frame_packet"); frame(j, packet); j.raw("}}");
  return j.ok ? j.used : 0;
}
std::size_t format_frame_packet_json(std::span<char> out, const FramePacket &packet) noexcept {
  if (packet.stage_count > 5) return 0;
  Json j{out}; j.raw("{"); frame(j, packet); j.raw("}"); return j.ok ? j.used : 0;
}

std::size_t format_capture_json(std::span<char> out, const CaptureMetadata &m,
    const EvaluateContractSnapshot *contract, const ResourceInspection *inspection,
    const BarrierCapture *barriers, const PresentationCreation *presentation, const FramePacket *packet) noexcept {
  if (packet && packet->stage_count > 5) return 0;
  if (barriers) {
    const auto &t = barriers->depth_timeline;
    if (t.retained > t.recent.size() || t.controls_retained > t.control_events.size() ||
        t.queues_retained > t.queue_events.size() || t.list_count > t.submitted_lists.size()) return 0;
    for (const auto &r : barriers->resources) if (r.retained > r.recent.size()) return 0;
    for (unsigned i = 0; i < t.queues_retained; ++i) {
      const auto &c = t.queue_events[i];
      if (c.kind == BarrierCallKind::execute && (c.first > t.list_count || c.count > t.list_count - c.first)) return 0;
    }
  }
  Json j{out}; j.raw("{"); j.n("schema", kCaptureMailboxSchema); j.n("producer", kCaptureMailboxProducer);
  j.n("evaluate_index", m.evaluate_index); j.n("sequence", m.sequence); j.n("tick", m.tick); j.n("thread", m.thread); j.n("caller", m.caller);
  j.n("token", m.token); j.n("result", m.result); j.b("committed", m.committed);
  j.b("command_verified", m.command_verified); j.b("boundary_clean", m.boundary_clean);
  j.b("handle_record_verified", m.handle_record_verified); j.b("handle_binding_matched", m.handle_binding_matched);
  j.b("has_contract", contract != nullptr); j.b("has_inspection", inspection != nullptr);
  j.b("has_barriers", barriers != nullptr); j.b("has_presentation", presentation != nullptr);
  j.b("has_frame_packet", packet != nullptr);
  if (packet) { j.object("frame_packet"); frame(j, *packet); j.raw("}"); }
  if (contract) {
    const auto &s = *contract; j.object("contract");
    j.n("queries", s.summary.queries_attempted); j.n("stop", static_cast<unsigned>(s.summary.stop_reason));
    j.b("complete", s.summary.complete); j.b("fault", s.summary.fault_latched);
    j.array("resources");
    for (const auto &r : s.resources) {
      j.item(); j.n("key", static_cast<unsigned>(r.key)); j.n("result", r.getter_result); j.n("value", r.value);
      j.b("attempted", r.query_attempted); j.b("completed", r.callback_completed); j.b("present", r.present); j.b("null", r.value_is_null); j.raw("}");
    }
    j.raw("]"); j.array("floats");
    for (const auto &r : s.floats) {
      j.item(); j.n("key", static_cast<unsigned>(r.key)); j.n("result", r.getter_result);
      j.n("bits", std::bit_cast<std::uint32_t>(r.value)); j.b("attempted", r.query_attempted);
      j.b("completed", r.callback_completed); j.b("present", r.present); j.raw("}");
    }
    j.raw("]"); j.array("integers");
    for (const auto &r : s.signed_values) {
      j.item(); j.n("key", static_cast<unsigned>(r.key)); j.n("result", r.getter_result); j.s("value", r.value);
      j.b("attempted", r.query_attempted); j.b("completed", r.callback_completed); j.b("present", r.present); j.raw("}");
    }
    j.raw("]}");
  }
  if (inspection) {
    const auto &s = *inspection; j.object("inspection");
    j.n("stop", static_cast<unsigned>(s.stop)); j.n("command_device_result", static_cast<std::uint32_t>(s.command_device_result));
    j.n("command_identity_result", static_cast<std::uint32_t>(s.command_identity_result)); j.n("command_type", s.command_type);
    j.n("adapter_low", s.adapter_luid.LowPart); j.s("adapter_high", s.adapter_luid.HighPart);
    j.n("calls", s.calls); j.n("references", s.references_acquired); j.n("release_attempts", s.releases_attempted); j.n("release_completions", s.releases_completed);
    j.b("contract_valid", s.contract_valid); j.b("command_identity_verified", s.command_identity_verified);
    j.b("adapter_available", s.adapter_available); j.b("complete", s.complete); j.b("fault", s.fault_latched);
    j.array("resources");
    for (const auto &r : s.resources) {
      const auto &d = r.description; j.item(); j.n("key", static_cast<unsigned>(r.key));
      j.n("interface_result", static_cast<std::uint32_t>(r.interface_result)); j.n("device_result", static_cast<std::uint32_t>(r.device_result));
      j.b("attempted", r.attempted); j.b("interface_verified", r.interface_verified); j.b("description_available", r.description_available); j.b("same_device", r.same_device);
      j.n("dimension", d.Dimension); j.n("alignment", d.Alignment); j.n("width", d.Width); j.n("height", d.Height);
      j.n("array_or_depth", d.DepthOrArraySize); j.n("mips", d.MipLevels); j.n("format", d.Format);
      j.n("samples", d.SampleDesc.Count); j.n("quality", d.SampleDesc.Quality); j.n("layout", d.Layout); j.n("flags", d.Flags); j.raw("}");
    }
    j.raw("]}");
  }
  if (barriers) {
    const auto &s = *barriers; j.object("barriers");
    j.n("command", s.command); j.n("evaluate_thread", s.evaluate_thread); j.n("calls", s.calls); j.n("descriptions", s.descriptions);
    j.n("matching_calls", s.matching_calls); j.n("resets", s.resets); j.n("epoch_sequence", s.epoch_sequence);
    j.b("installed", s.installed); j.b("armed", s.armed); j.b("finished", s.finished); j.b("methods_match", s.methods_match);
    j.b("fault", s.faulted); j.b("overflow", s.overflow); j.b("contention", s.contention); j.b("expired", s.expired); j.b("in_flight", s.in_flight);
    j.b("contract_valid", s.contract_valid); j.b("coverage_complete", s.coverage_complete); j.b("closed", s.closed); j.b("bundle_seen", s.bundle_seen); j.b("reset_failed", s.reset_failed);
    j.array("resources");
    for (const auto &r : s.resources) {
      j.item(); j.n("key", static_cast<unsigned>(r.key)); j.n("transitions", r.transitions); j.n("uav", r.uav); j.n("aliasing", r.aliasing);
      j.n("matches", r.matches); j.n("retained", r.retained); j.n("last_state", r.last_declared_state);
      j.b("present", r.present); j.b("ambiguous", r.ambiguous); j.b("continuity_mismatch", r.continuity_mismatch);
      j.b("declaration_available", r.declaration_available); j.b("expected_state_matches", r.expected_state_matches);
      j.array("recent"); for (unsigned i = 0; i < r.retained; ++i) { j.item(); match(j, r.recent[i]); j.raw("}"); } j.raw("]}");
    }
    j.raw("]"); const auto &t = s.depth_timeline; j.object("depth_timeline");
    j.n("matches", t.matches); j.n("retained", t.retained); j.n("controls", t.controls); j.n("controls_retained", t.controls_retained);
    j.n("queues", t.queues); j.n("queues_retained", t.queues_retained); j.n("list_count", t.list_count);
    j.b("truncated", t.truncated); j.b("malformed", t.malformed);
    j.array("recent"); for (unsigned i = 0; i < t.retained; ++i) { j.item(); match(j, t.recent[i]); j.raw("}"); } j.raw("]");
    j.array("controls_events"); for (unsigned i = 0; i < t.controls_retained; ++i) { j.item(); call(j, t.control_events[i]); j.raw("}"); } j.raw("]");
    j.array("queue_events"); for (unsigned i = 0; i < t.queues_retained; ++i) { j.item(); call(j, t.queue_events[i]); j.raw("}"); } j.raw("]");
    j.array("submitted_lists"); for (unsigned i = 0; i < t.list_count; ++i) { if (i) j.raw(","); j.number(t.submitted_lists[i]); } j.raw("]}}");
  }
  if (presentation) {
    const auto &p = *presentation; j.object("presentation"); j.n("format", p.format); j.n("width", p.width); j.n("height", p.height);
    j.n("buffers", p.buffers); j.n("flags", p.flags); j.n("adapter_low", p.adapter_luid.LowPart); j.s("adapter_high", p.adapter_luid.HighPart);
    j.n("observed", p.observed); j.n("matching", p.matching); j.b("installed", p.installed); j.b("fault", p.fault_latched);
    j.b("overflow", p.overflow); j.b("in_flight", p.in_flight); j.b("available", p.available); j.raw("}");
  }
  j.b("gpu_state_verified", false); j.b("gpu_completion_observed", false); j.raw("}");
  return j.ok ? j.used : 0;
}

std::uint64_t process_creation_stamp(HANDLE process) noexcept {
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return 0;
  return (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
}
std::wstring capture_mailbox_name(DWORD pid, std::uint64_t creation, unsigned index) {
  return L"Local\\MhwDlssBridgeCapture-v1-" + std::to_wstring(pid) + L"-" + std::to_wstring(creation) + (index ? L"-slot-" + std::to_wstring(index) : L"");
}
CaptureMailboxWriter::~CaptureMailboxWriter() { if (view_) UnmapViewOfFile(view_); if (mapping_) CloseHandle(mapping_); }
bool CaptureMailboxWriter::create(unsigned index) noexcept {
  if (mapping_ || view_ || index >= 3) return false;
  try {
    const auto pid = GetCurrentProcessId(); const auto creation = process_creation_stamp(GetCurrentProcess());
    if (!creation) return false;
    const auto name = capture_mailbox_name(pid, creation, index);
    const auto map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Mailbox), name.c_str());
    const auto error = GetLastError();
    if (!map) return false;
    if (error == ERROR_ALREADY_EXISTS) { CloseHandle(map); return false; }
    auto view = MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, sizeof(Mailbox));
    if (!view) { CloseHandle(map); return false; }
    auto &m = *new(view) Mailbox{};
    m.magic = magic; m.schema = kCaptureMailboxSchema; m.producer = kCaptureMailboxProducer;
    m.bytes = sizeof(Mailbox); m.pid = pid; m.creation = creation; m.reserved = index;
    state(m).store(static_cast<unsigned>(MailboxState::idle), std::memory_order_release);
    mapping_ = map; view_ = view; return true;
  } catch (...) { return false; }
}
bool CaptureMailboxWriter::enter() noexcept {
  if (!view_) return false;
  auto expected = static_cast<unsigned>(MailboxState::idle);
  return state(*static_cast<Mailbox *>(view_)).compare_exchange_strong(expected,
      static_cast<unsigned>(MailboxState::entered), std::memory_order_acq_rel);
}
bool CaptureMailboxWriter::publish(const CaptureMetadata &metadata, const EvaluateContractSnapshot *contract,
    const ResourceInspection *inspection, const BarrierCapture *barriers, const PresentationCreation *presentation, const FramePacket *packet) noexcept {
  if (!view_) return false;
  auto &m = *static_cast<Mailbox *>(view_); auto expected = static_cast<unsigned>(MailboxState::entered);
  if (!state(m).compare_exchange_strong(expected, static_cast<unsigned>(MailboxState::writing), std::memory_order_acq_rel)) return false;
  const auto length = metadata.evaluate_index == m.reserved ?
      format_capture_json(m.json, metadata, contract, inspection, barriers, presentation, packet) : 0;
  m.qualified = metadata.committed && metadata.command_verified && metadata.boundary_clean &&
      metadata.handle_record_verified && metadata.handle_binding_matched && contract &&
      contract->summary.complete && !contract->summary.fault_latched && inspection && inspection->complete &&
      !inspection->fault_latched && barriers && barriers->coverage_complete && !barriers->closed &&
      !barriers->bundle_seen && !barriers->reset_failed ? 1U : 0U;
  m.inputs_verified = metadata.committed && metadata.command_verified && metadata.boundary_clean &&
      metadata.handle_record_verified && metadata.handle_binding_matched && contract && contract->summary.complete &&
      !contract->summary.fault_latched && contract->summary.queries_attempted == kEvaluateQueryCount &&
      inspection && inspection->complete && !inspection->fault_latched && inspection->command_identity_verified &&
      barriers && barriers->contract_valid && barriers->installed && barriers->armed && barriers->finished &&
      barriers->methods_match && !barriers->faulted && !barriers->overflow && !barriers->contention && !barriers->expired &&
      barriers->command && barriers->evaluate_thread == metadata.thread ? 1U : 0U;
  m.binding.index = metadata.evaluate_index; m.binding.thread = metadata.thread;
  if (packet && (!packet->evaluate_bound || packet->rejection)) m.qualified = m.inputs_verified = 0;
  if (barriers) m.binding.command = barriers->command;
  if (contract) for (unsigned i = 0; i < m.binding.resources.size(); ++i) m.binding.resources[i] = contract->resources[i].value;
  m.length = static_cast<std::uint32_t>(length); m.checksum = payload_checksum(m, {m.json.data(), length});
  state(m).store(static_cast<unsigned>(length ? MailboxState::published : MailboxState::failed), std::memory_order_release);
  return length != 0;
}
CaptureMailboxReader::~CaptureMailboxReader() { if (view_) UnmapViewOfFile(view_); if (mapping_) CloseHandle(mapping_); }
bool CaptureMailboxReader::open(DWORD pid, std::uint64_t creation, unsigned index) {
  if (mapping_ || view_ || !pid || !creation || index >= 3) return false;
  const auto name = capture_mailbox_name(pid, creation, index);
  const auto map = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
  if (!map) return false;
  const auto view = MapViewOfFile(map, FILE_MAP_READ, 0, 0, sizeof(Mailbox));
  if (!view) { CloseHandle(map); return false; }
  const auto &m = *static_cast<const Mailbox *>(view);
  const auto current = state(m).load(std::memory_order_acquire);
  if (current < static_cast<unsigned>(MailboxState::idle) || current > static_cast<unsigned>(MailboxState::failed) || !header(m, pid, creation, index)) {
    UnmapViewOfFile(view); CloseHandle(map); return false;
  }
  mapping_ = map; view_ = view; pid_ = pid; creation_ = creation; index_ = index; return true;
}
MailboxRead CaptureMailboxReader::read() const {
  if (!view_) return {};
  const auto &m = *static_cast<const Mailbox *>(view_);
  const auto current = state(m).load(std::memory_order_acquire);
  if (current < static_cast<unsigned>(MailboxState::idle) || current > static_cast<unsigned>(MailboxState::failed) || !header(m, pid_, creation_, index_)) return {MailboxState::invalid, {}};
  const auto status = static_cast<MailboxState>(current);
  if (status != MailboxState::published) return {status, {}};
  const auto size = m.length, expected = m.checksum;
  if (size < 2 || size > m.json.size()) return {MailboxState::invalid, {}};
  std::string copy(m.json.data(), size);
  if (state(m).load(std::memory_order_acquire) != current || m.length != size || m.checksum != expected ||
      !header(m, pid_, creation_, index_) || payload_checksum(m, copy) != expected || m.qualified > 1 || m.inputs_verified > 1 || m.binding.index != index_ || copy.front() != '{' || copy.back() != '}') return {MailboxState::invalid, {}};
  for (const unsigned char c : copy) if (c < 32 || c > 126) return {MailboxState::invalid, {}};
  return {status, std::move(copy), m.qualified == 1, m.inputs_verified == 1, m.binding};
}
} // namespace mhw::bridge_probe

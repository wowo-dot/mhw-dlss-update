#include "native_depth_probe.hpp"
#include "native_taa_fallback.hpp"
#include <MinHook.h>
#include <atomic>
#include <intrin.h>
#include <mutex>
#include <windows.h>

extern "C" std::uintptr_t native_depth_helper_entry(std::uintptr_t,
                                                    std::uintptr_t,
                                                    std::uint16_t) noexcept;
namespace mhw::bridge_probe {
namespace {
using Helper = std::uintptr_t(__fastcall *)(std::uintptr_t, std::uintptr_t,
                                            std::uint16_t);
using Consumer = void(__fastcall *)(std::uintptr_t, std::uintptr_t);
Helper original_helper{};
Consumer original_consumer{};
std::uintptr_t host{}, helper_target{}, consumer_target{}, expected_consumer{};
std::array<std::uintptr_t, 4> helper_callers{};
std::mutex gate_mutex;
DepthWorkGate gate;
FramePacketGate frames;
JitterSequence jitter_sequence; // gate_mutex, scalar copies only
std::atomic<bool> publication_ready{};
unsigned producer_thread{}; // gate_mutex
std::atomic<unsigned> install_status{};
unsigned rejection_stage{}, capture_checkpoint{}, capture_reads{},
    tracker_state{}; // gate_mutex
void observe_capture(const DepthWorkCapture &c) noexcept {
  capture_checkpoint = c.checkpoint;
  capture_reads = c.reads;
}
void rejected(unsigned stage) noexcept {
  if (!rejection_stage)
    rejection_stage = stage;
}
std::atomic<std::uint64_t> producer_admitted{}, preparation_calls{},
    depth_corrected{}, preparation_rejected{}, consumer_admitted{},
    consumer_rejected{}, consumer_completed{};
thread_local bool inside_consumer{};
} // namespace

void observe_native_jitter_sequence(std::uintptr_t args, bool with_wrapper, std::uint64_t tick) noexcept {
  if (install_status.load(std::memory_order_acquire) != 4) return;
  const std::unique_lock lock(gate_mutex, std::try_to_lock);
  if (!lock.owns_lock()) return;
  // Read the production clock after acquiring the lock: an earlier timestamp
  // could precede another thread's first sample while this thread is preempted.
  if (!tick) tick = GetTickCount64(); // nonzero is used only by owned fixtures
  JitterSample sample;
  if (!jitter_sequence.reserve(tick, with_wrapper, sample)) return;
  sample.thread = GetCurrentThreadId();
  auto &p = sample.frame;
  p = capture_producer_frame(host, args, read_bounded_scalar, nullptr);
  if (p.valid) {
    sample.gates = capture_jitter_gates(host, args, p, read_bounded_scalar, nullptr);
    p.projection = capture_projection_snapshot(host,
        {p.renderer, p.recording_context, p.view, p.table, p.frame, p.epoch, p.bank, p.async_mode},
        read_bounded_scalar, nullptr);
    const auto after = capture_jitter_gates(host, args, p, read_bounded_scalar, nullptr);
    if (sample.gates != after) { sample.gates.valid = false; sample.gates.predicate = false; sample.gates.rejection = 3; }
    if (!frame_anchor_matches(host, p, read_bounded_scalar, nullptr)) { p.valid = false; p.checkpoint = 4; }
  }
  jitter_sequence.commit(sample);
}
bool native_jitter_sample(unsigned index, JitterSample &out) noexcept {
  const std::lock_guard lock(gate_mutex); return jitter_sequence.sample(index, out);
}
JitterSequenceState native_jitter_sequence_state(std::uint64_t tick) noexcept {
  const std::lock_guard lock(gate_mutex); jitter_sequence.poll(tick ? tick : GetTickCount64()); return jitter_sequence.state();
}
void close_native_jitter_sequence() noexcept {
  const std::lock_guard lock(gate_mutex); jitter_sequence.close();
}
bool arm_native_jitter_scene(std::uint64_t tick) noexcept {
  const std::lock_guard lock(gate_mutex);
  return jitter_sequence.arm_scene(tick ? tick : GetTickCount64());
}

bool admit_native_depth_producer(std::uintptr_t args) noexcept {
  const std::lock_guard lock(gate_mutex);
  if (install_status.load(std::memory_order_acquire) != 4 ||
      gate.phase() != DepthWorkPhase::idle)
    return false;
  auto frame = capture_producer_frame(host, args, read_bounded_scalar, nullptr);
  auto capture =
      capture_producer_depth_work(host, args, read_bounded_scalar, nullptr);
  if (frame.valid) frame.projection = capture_projection_snapshot(host,
      {frame.renderer, frame.recording_context, frame.view, frame.table, frame.frame, frame.epoch, frame.bank, frame.async_mode},
      read_bounded_scalar, nullptr);
  observe_capture(capture);
  // The frame bracket also covers resource-owner discovery, not just scalars.
  if (frame.valid && !frame_anchor_matches(host, frame, read_bounded_scalar, nullptr)) {
    frame.valid = false; frame.checkpoint = 4;
  }
  if (capture.valid) capture.valid = frames.queue(frame, capture.identity);
  if (!gate.queue(capture)) {
    rejected(1);
    return false;
  }
  producer_admitted.fetch_add(1, std::memory_order_relaxed);
  producer_thread = GetCurrentThreadId();
  return true;
}

void publish_native_depth_producer(std::uintptr_t entry) noexcept {
  const std::lock_guard lock(gate_mutex);
  PublicationReceipt receipt{}; DepthWorkCapture work{};
  if (gate.phase() == DepthWorkPhase::queued && producer_thread == GetCurrentThreadId()) {
    receipt = capture_publication(host, frames.packet().producer, entry, read_bounded_scalar, nullptr);
    if (receipt.valid) {
      work = capture_prepared_depth_work(host, receipt.payload, receipt.descriptor, read_bounded_scalar, nullptr);
      // Bracket resource discovery with the same bounded publication snapshot.
      const auto after = capture_publication(host, frames.packet().producer, entry, read_bounded_scalar, nullptr);
      if (receipt != after) { receipt.valid = false; receipt.checkpoint = 4; }
    }
  }
  if (!frames.publish(receipt, work)) { gate.reject(); rejected(4); return; }
  publication_ready.store(true, std::memory_order_release);
}

// The assembly entry captures the caller's nonvolatile payload/descriptor
// registers before a compiler prologue can repurpose them. It has unwind info.
extern "C" __declspec(noinline) std::uintptr_t __fastcall
native_depth_helper_dispatch(std::uintptr_t context, std::uintptr_t supplied,
                             std::uint16_t request, std::uintptr_t caller,
                             std::uintptr_t payload,
                             std::uintptr_t descriptor) noexcept {
  unsigned ordinal{};
  while (ordinal < 4 && helper_callers[ordinal] != caller)
    ++ordinal;
  if (ordinal == 4)
    return original_helper(context, supplied, request);
  // Sample before taking the existing policy mutex. A callback that arrives
  // before publication cannot wait its way into acceptance.
  const bool had_publication = publication_ready.load(std::memory_order_acquire);
  const auto last_error = GetLastError();
  std::uintptr_t result{};
  {
    // Original helper contains no external call/re-entry. Keep this lock until
    // it returns so a consumer cannot observe prepared before bookkeeping ends.
    const std::lock_guard lock(gate_mutex);
    preparation_calls.fetch_add(1, std::memory_order_relaxed);
    if (!had_publication) { frames.reject(5); gate.reject(); rejected(5); }
    DepthWorkCapture capture{};
    if (gate.phase() == DepthWorkPhase::queued ||
        gate.phase() == DepthWorkPhase::preparing) {
      capture = capture_prepared_depth_work(host, payload, descriptor,
                                            read_bounded_scalar, nullptr);
      observe_capture(capture);
    }
    const bool tracker =
        ordinal != 2 ||
        (capture.valid && depth_tracking_context_verified(
                              context, descriptor, capture.identity.textures[3],
                              read_bounded_scalar, nullptr, &tracker_state));
    if (capture.valid) capture.valid = frames.prepare(ordinal,
        capture_stage_frame(host, payload, descriptor, context, read_bounded_scalar, nullptr));
    const auto backing =
        gate.prepare(ordinal, capture, payload, descriptor, context,
                     GetCurrentThreadId(), supplied, request, tracker);
    if (backing) {
      SetLastError(last_error);
      result = original_helper(context, backing, request);
      if (ordinal == 2)
        depth_corrected.fetch_add(1, std::memory_order_relaxed);
    } else {
      rejected(10 + ordinal);
      preparation_rejected.fetch_add(1, std::memory_order_relaxed);
    }
  }
  SetLastError(last_error);
  return result;
}

extern "C" __declspec(dllexport) __declspec(noinline) void __fastcall
native_depth_consumer(std::uintptr_t payload,
                      std::uintptr_t command_list) noexcept {
  if (reinterpret_cast<std::uintptr_t>(_ReturnAddress()) != expected_consumer) {
    original_consumer(payload, command_list);
    return;
  }
  const auto last_error = GetLastError();
  bool admitted{};
  {
    const std::lock_guard lock(gate_mutex);
    DepthWorkCapture capture{};
    if (gate.phase() == DepthWorkPhase::prepared) {
      capture = capture_consumer_depth_work(host, payload, read_bounded_scalar,
                                            nullptr);
      observe_capture(capture);
    }
    if (capture.valid) capture.valid = frames.consume(
        capture_stage_frame(host, payload, 0, 0, read_bounded_scalar, nullptr), command_list);
    admitted =
        command_list && !inside_consumer && gate.consume(capture, payload);
    if (!admitted) {
      rejected(20);
      gate.reject();
    }
  }
  if (admitted) {
    consumer_admitted.fetch_add(1, std::memory_order_relaxed);
    inside_consumer = true;
    SetLastError(last_error);
    original_consumer(payload, command_list);
    inside_consumer = false;
    const std::lock_guard lock(gate_mutex);
    gate.finish();
    consumer_completed.fetch_add(1, std::memory_order_relaxed);
  } else
    consumer_rejected.fetch_add(1, std::memory_order_relaxed);
  SetLastError(last_error);
}
bool native_depth_evaluate_permitted() noexcept { return inside_consumer; }
FramePacket native_depth_frame_packet() noexcept {
  const std::lock_guard lock(gate_mutex); return frames.packet();
}
bool bind_native_depth_evaluate(std::uintptr_t command, const EvaluateContractSnapshot &input) noexcept {
  const std::lock_guard lock(gate_mutex);
  if (!inside_consumer || gate.phase() != DepthWorkPhase::consuming) return false;
  const auto payload = frames.packet().stages[0].payload;
  return frames.evaluate(command, capture_stage_frame(host, payload, 0, 0, read_bounded_scalar, nullptr), input);
}

namespace {
bool install(std::uintptr_t base, std::uintptr_t helper,
             std::uintptr_t consumer,
             const std::array<std::uintptr_t, 4> &callers,
             std::uintptr_t consumer_caller) noexcept {
  const auto status = MH_Initialize();
  if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    return false;
  void *helper_trampoline{}, *consumer_trampoline{};
  if (MH_CreateHook(reinterpret_cast<void *>(helper),
                    reinterpret_cast<void *>(&native_depth_helper_entry),
                    &helper_trampoline) != MH_OK)
    return false;
  if (MH_CreateHook(reinterpret_cast<void *>(consumer),
                    reinterpret_cast<void *>(&native_depth_consumer),
                    &consumer_trampoline) != MH_OK) {
    MH_RemoveHook(reinterpret_cast<void *>(helper));
    return false;
  }
  if (!helper_trampoline || !consumer_trampoline)
    return false;
  original_helper = reinterpret_cast<Helper>(helper_trampoline);
  original_consumer = reinterpret_cast<Consumer>(consumer_trampoline);
  host = base;
  helper_target = helper;
  consumer_target = consumer;
  helper_callers = callers;
  expected_consumer = consumer_caller;
  // Consumer must be guarded before preparation is changed. No feature token
  // is issued until this and the producer hook are both installed.
  if (MH_EnableHook(reinterpret_cast<void *>(consumer)) != MH_OK ||
      MH_EnableHook(reinterpret_cast<void *>(helper)) != MH_OK)
    return false;
  install_status.store(4, std::memory_order_release);
  return true;
}
} // namespace
bool install_native_depth_probe(std::uintptr_t base) noexcept {
  if (install_status.load(std::memory_order_acquire) == 4)
    return host == base;
  install_status.store(2, std::memory_order_release);
  if (!native_code_span_verified(base, 0x23cabf0, 99,
          "5c63fd6c587c250e0fb09f02c2cf776fb44ed90944e90d1b98c80a700036d433") ||
      !native_code_span_verified(base, 0x229a080, 1428,
          "961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0") ||
      !native_code_span_verified(base, 0x2293280, 489,
          "576813bea02244e8970c592cf2f2a10e74335cb893c1402f560cf3eb4fa9ad64") ||
      !native_code_span_verified(base, 0x23ca8b0, 206,
          "b27d637cadf610c779092c55dc94cda7812d4525fbb864fc26acad0b91620edc") ||
      !native_code_span_verified(base, 0x23d2370, 1047,
          "0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3")) return false;
  // CPU projection snapshot layouts and transform are exact-build gated.
  if (!native_code_span_verified(base, 0x2391320, 198,
          "46d851b5efa9af23eae20385a0d5cbd44b7618366e21fa2577e82fc0eddf7c4f") ||
      !native_code_span_verified(base, 0x2391830, 1778,
          "e977a8a458ad4edc53984366fdc3254e166081a8a0e8733cd2feb32a5a3b0f10")) return false;
  if (!native_code_span_verified(base, 0x228d7a0, 372,
          "6fd1afa8c8c0f2027f5cc7263443907484cbcaf750c0a77babf320027148b9ff") ||
      !native_code_span_verified(base, 0x1ac32b0, 739,
          "05958a8a632c195a172389e85bc6280413b32b99196d7c4d4ae10910cc137f2b") ||
      !native_code_span_verified(base, 0x228f390, 24,
          "5123862eb70e10acd2c695bc76f53b3b62c3d2ef6d34a8b5f3ff6c8a88644414") ||
      !native_code_span_verified(base, 0x23931e0, 124,
          "88aa2c037b51855a2e516f683ec68c1a00565edc5f78741e8db71783e0b7e488") ||
      !native_code_span_verified(base, 0x228f3b0, 2000,
          "df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373") ||
      !native_code_span_verified(base, 0x23d0e70, 5321,
          "872aa5439077ddac98d887f7acd1666e801e9c7db3e43bc2b433f1841b3da509") ||
      !native_code_span_verified(base, 0x23ccc50, 203,
          "1419b53c5c02532d53ea1c5125134f6b9749ca44fde0be91574e18f2ce808127") ||
      !native_code_span_verified(base, 0x28e280, 1030,
          "e23d74268bd6aa3b214f7e0c0cacf977d2c68fe719ffabf1ef24c2236a592713") ||
      !native_code_span_verified(base, 0x1fad4c0, 211,
          "428cc3b640b19bc38892e5ada6299dae077a7563e49e2b6d87d8eb887019bade") ||
      !native_code_span_verified(base, 0x227ff00, 1116,
          "b7915ccf1b16b06f871d59535512639eec505d4eb1eab6a5732093e52d012353")) return false;
  // These read-only source locations are derived from exact accessor and
  // dispatch code, not assumed from property names or a different capture.
  if (!native_code_span_verified(base, 0x2299390, 7,
          "a64d4fc3d032cd443d7083fb7a71ba2e8a99abb9a9847c4c9b3c1732be02f638") ||
      !native_code_span_verified(base, 0x227f7b0, 53,
          "9cf3d0002a218375575109acf8bd205121d1a93a2144108dd30b6d0bef7f5f1c") ||
      !native_code_span_verified(base, 0x227db60, 52,
          "d9a8d5c90857455446e900ceae4ce7a176b875ea02d2bdb8c4ff34b03536e5e6") ||
      !native_code_span_verified(base, 0x23c6c5d, 232,
          "2440f660470e9e7b843475cfe0e37e297ebc1586fb66a49394eaacf2e8a59c4b")) return false;
  if (!native_code_span_verified(
          base, 0x22927f0, 0x2d1,
          "d27e289f7fdc9f5205e6e8e61615586366a18d0e725235801690a448cddd8ba2") ||
      !native_code_span_verified(
          base, 0x229cee0, 7756,
          "11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3") ||
      !native_code_span_verified(
          base, 0x2383cf0, 503,
          "9a5dbaef62f4d190685cc92d30531ef7dbd72c7cb8241fd7f20fa94d6ac93ea5") ||
      !native_code_span_verified(
          base, 0x259dae0, 918,
          "a0ba3e63df0026b40bd545d9a7668b899fbcef7fd83d5e0898cd9a31bd12338d"))
    return false;
  install_status.store(3, std::memory_order_release);
  return install(
      base, base + 0x22927f0, base + 0x2383cf0,
      {base + 0x229e86b, base + 0x229e883, base + 0x229e89e, base + 0x229e8b3},
      base + 0x259dcd7);
}
NativeDepthProbeStats native_depth_probe_stats() noexcept {
  const std::lock_guard lock(gate_mutex);
  return {install_status.load(),
          static_cast<unsigned>(gate.phase()),
          rejection_stage,
          capture_checkpoint,
          capture_reads,
          tracker_state,
          producer_admitted.load(),
          preparation_calls.load(),
          depth_corrected.load(),
          preparation_rejected.load(),
          consumer_admitted.load(),
          consumer_rejected.load(),
          consumer_completed.load()};
}
#ifdef MHW_OWNED_DEPTH_TEST
bool install_owned_depth_probe(std::uintptr_t base, std::uintptr_t helper,
                               std::uintptr_t consumer,
                               const std::array<std::uintptr_t, 4> &callers,
                               std::uintptr_t consumer_caller) noexcept {
  return install(base, helper, consumer, callers, consumer_caller);
}
bool remove_owned_depth_probe() noexcept {
  return MH_DisableHook(reinterpret_cast<void *>(helper_target)) == MH_OK &&
         MH_DisableHook(reinterpret_cast<void *>(consumer_target)) == MH_OK &&
         MH_RemoveHook(reinterpret_cast<void *>(helper_target)) == MH_OK &&
         MH_RemoveHook(reinterpret_cast<void *>(consumer_target)) == MH_OK;
}
#endif
} // namespace mhw::bridge_probe

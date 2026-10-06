#include "bridge_policy.hpp"
#include "depth_work.hpp"
#include "handle_lifetime.hpp"
#include "native_depth_probe.hpp"
#include "native_taa_fallback.hpp"
#include "optimal_settings.hpp"
#include "capture_mailbox.hpp"
#include <array>
#include <bit>
#include <limits>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <thread>
#include <windows.h>
namespace b = mhw::bridge_probe;
using A = std::uintptr_t;
void check(bool v, const char *why) {
  if (!v)
    throw std::runtime_error(why);
}
template <class T, class B> void put(B &buffer, unsigned offset, T v) {
  std::memcpy(buffer.data() + offset, &v, sizeof(v));
}
template <class B> A at(B &buffer) {
  return reinterpret_cast<A>(buffer.data());
}
struct Fixture {
  A base{};
  std::array<std::array<std::byte, 280>, 4> owners{};
  std::array<std::array<std::byte, 128>, 4> backings{};
  std::array<std::array<std::byte, 48>, 4> trackers{};
  std::array<std::byte, 40> active{};
  std::vector<std::byte> renderer = std::vector<std::byte>(0x7b430);
  std::array<std::byte, 0xe7e0> jitter_table{};
  std::array<std::byte, 72> projection_manager{};
  std::array<std::byte, 32> projection_metadata{};
  std::array<A, 2> projection_metadata_table{};
  std::vector<std::byte> projection_constants = std::vector<std::byte>(128);
  std::array<std::int32_t, 4> rect{0, 0, 2560, 1440};
  unsigned char reset_input{1};
  std::array<std::byte, 1600> view{};
  std::array<std::byte, 176> container{};
  std::array<std::byte, 448> layer{};
  std::array<std::byte, 64> payload{}, descriptor{};
  std::vector<std::byte> entries = std::vector<std::byte>(96);
  std::array<A, 8> args{};
  std::array<A, 7> context{};
  A wrapper{123}, view_pointer{}, output_index{}, epoch{7},
      descriptor_pointer{};
  A next_index{1}, recording_context{456};
  unsigned read_calls{}, fail_read{};
  unsigned frame_reads{};
  bool mutate_frame{};
  A mutate_address{};
  unsigned mutate_visits{};
  Fixture() {
    base = reinterpret_cast<A>(
        VirtualAlloc(nullptr, 0x51c5000, MEM_RESERVE, PAGE_NOACCESS));
    check(base != 0, "reserve sparse owned identity span");
    check(VirtualAlloc(reinterpret_cast<void *>(base + 0x51c4000), 0x1000,
                       MEM_COMMIT, PAGE_READWRITE) != nullptr,
          "commit owned renderer slot");
    check(VirtualAlloc(reinterpret_cast<void *>(base + 0x4be7000), 0x1000,
                       MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit owned shader parameter slot");
    check(VirtualAlloc(reinterpret_cast<void *>(base + 0x503c000), 0x1000,
                       MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit owned reset vector");
    const auto renderer_address = at(renderer);
    std::memcpy(reinterpret_cast<void *>(base + 0x51c4480), &renderer_address,
                sizeof(A));
    const auto table_address = at(jitter_table);
    std::memcpy(reinterpret_cast<void *>(base + 0x51c4368), &table_address, sizeof(A));
    put(renderer, 0x1f210, std::uint64_t{6});
    put(renderer, 0x1f208, epoch); put(renderer, 0x1f200, 0U);
    put(renderer, 0x93, static_cast<unsigned char>(1));
    recording_context = at(renderer) + 0x710;
    put(renderer, 0x710 + 0x2b78, at(entries));
    put(renderer, 0x710 + 0x2b80, 1U); put(renderer, 0x710 + 0x2b84, 6U);
    put(entries, 0, 17U); put(entries, 8, at(descriptor));
    put(renderer, 0x7b424, 1U);
    put(jitter_table, 0xe740 + 6 * 8, 0.25f);
    put(jitter_table, 0xe740 + 6 * 8 + 4, -0.125f);
    view_pointer = at(view);
    put(renderer, 128080, view_pointer);
    const auto manager_address = at(projection_manager);
    std::memcpy(reinterpret_cast<void *>(base + 0x51c4400), &manager_address, sizeof(A));
    const unsigned cb_id = 1, proj_id = 0xf00, plain_id = 0x10f00;
    std::memcpy(reinterpret_cast<void *>(base + 0x4be75f4), &cb_id, 4);
    std::memcpy(reinterpret_cast<void *>(base + 0x4be7604), &proj_id, 4);
    std::memcpy(reinterpret_cast<void *>(base + 0x4be763c), &plain_id, 4);
    put(projection_manager, 0, base + 0x33f82a8); put(projection_manager, 0x40, static_cast<unsigned>(epoch));
    put(projection_manager, 0x38, .25f); put(projection_manager, 0x3c, -.125f);
    put(jitter_table, 0xe6a0 + 6 * 8, .25f); put(jitter_table, 0xe6a0 + 6 * 8 + 4, -.125f);
    projection_metadata_table[1] = at(projection_metadata); put(projection_metadata, 0x10, 32U);
    put(renderer, 0x710 + 0x2b18, at(projection_metadata_table));
    put(renderer, 0x710 + 0x718, at(projection_constants) | A{3});
    put(renderer, 0x710 + 0x230, 2560.f); put(renderer, 0x710 + 0x234, 1440.f);
    for (unsigned i = 0; i < 16; ++i) {
      const float v = i / 4 == i % 4 ? 1.f : 0.f;
      put(projection_constants, 4 * i, v); put(projection_constants, 64 + 4 * i, v);
    }
    put(projection_constants, 48, .25f / 2560.f); put(projection_constants, 52, -.125f / 1440.f);
    put(view, 1576, at(container));
    put(container, 0, base + 0x35884e0);
    put(container, 144, A{0x4444});
    put(container, 160, at(owners[3]));
    const unsigned kinds[]{5, 0, 0, 5},
        flags[]{0x200002, 2, 0x600002, 0x600000}, formats[]{34, 5, 34, 16};
    for (unsigned i = 0; i < 4; ++i) {
      put(owners[i], 0, base + 0x351c870);
      put(owners[i], 88, kinds[i]);
      put(owners[i], 96, at(backings[i]));
      put(owners[i], 112, flags[i]);
      put(owners[i], 136, 2560U | (1440U << 16));
      put(owners[i], 144, formats[i]);
      put(owners[i], 148, 0xffffffffU);
      put(backings[i], 32, A{0x1000 + i});
      put(backings[i], 112, at(trackers[i]));
      put(payload, 8 + 8 * i, at(owners[i]));
    }
    put(trackers[3], 8, at(active));
    put(trackers[3], 16, epoch);
    put(trackers[3], 26, std::uint16_t{8});
    put(active, 34, std::uint16_t{8});
    put(layer, 336, at(owners[0]));
    put(layer, 424, at(owners[1]));
    put(layer, 360, at(owners[2]));
    put(layer, 0, base + 0x351bda8); put(layer, 0xc, 8U); put(layer, 0x14, 0x12U);
    put(layer, 0x180, .95f); put(layer, 0x184, static_cast<unsigned char>(1));
    put(renderer, 0xa5, static_cast<unsigned char>(1));
    args = {reinterpret_cast<A>(&wrapper),
            at(layer),
            reinterpret_cast<A>(&recording_context),
            at(rect),
            reinterpret_cast<A>(&next_index),
            reinterpret_cast<A>(&output_index),
            reinterpret_cast<A>(&view_pointer),
            reinterpret_cast<A>(&reset_input)};
    put(payload, 0, wrapper);
    for (unsigned i = 0; i < 4; ++i) put(payload, 44 + i * 4, static_cast<float>(rect[i]));
    put(descriptor, 0, 23U);
    put(descriptor, 16, 3U);
    put(descriptor, 32, at(payload));
    descriptor_pointer = at(descriptor);
    context[0] = reinterpret_cast<A>(&descriptor_pointer);
    context[2] = reinterpret_cast<A>(&epoch);
  }
  ~Fixture() {
    if (base)
      VirtualFree(reinterpret_cast<void *>(base), 0, MEM_RELEASE);
  }
  static b::ScalarReadResult read(void *opaque, A address,
                                  std::span<std::byte> out) noexcept {
    auto &f = *static_cast<Fixture *>(opaque);
    if (++f.read_calls == f.fail_read)
      return b::ScalarReadResult::unreadable;
    if (f.mutate_address && address == f.mutate_address && ++f.mutate_visits == 2) {
      std::memcpy(out.data(), reinterpret_cast<void *>(address), out.size());
      out[0] ^= std::byte{1}; return b::ScalarReadResult::success;
    }
    if (f.mutate_frame && address == at(f.renderer) + 0x1f210 && ++f.frame_reads == 2)
      put(f.renderer, 0x1f210, std::uint64_t{7});
    return b::read_bounded_scalar(nullptr, address, out);
  }
  auto producer() {
    read_calls = 0;
    return b::capture_producer_depth_work(base, at(args), read, this);
  }
  auto prepared() {
    read_calls = 0;
    return b::capture_prepared_depth_work(base, at(payload), at(descriptor),
                                          read, this);
  }
  auto publication(const b::ProducerFrame &p, A returned = 0) {
    read_calls = 0;
    return b::capture_publication(base, p, returned ? returned : at(entries), read, this);
  }
};
Fixture *live{};
std::string_view scenario;
unsigned producer_calls{}, helper_calls{}, consumer_calls{};
std::array<A, 8> helper_backings{};
bool consumer_permit{};
extern "C" void mhw_depth_fixture_prepare(A, A, A);
extern "C" void mhw_depth_fixture_consume(A, A);
extern "C" A mhw_taa_fixture_call(A);
extern "C" unsigned char mhw_taa_fixture_return, mhw_depth_fixture_color_return,
    mhw_depth_fixture_motion_return, mhw_depth_fixture_depth_return,
    mhw_depth_fixture_output_return, mhw_depth_fixture_consumer_return;
extern "C" __declspec(noinline) A __fastcall
mhw_depth_fixture_helper(A, A backing, std::uint16_t) {
  if (helper_calls < helper_backings.size())
    helper_backings[helper_calls] = backing;
  ++helper_calls;
  return backing + 17;
}
extern "C" __declspec(noinline) void __fastcall mhw_depth_fixture_consumer(A,
                                                                           A) {
  ++consumer_calls;
  consumer_permit = b::native_depth_evaluate_permitted();
  if (consumer_permit) {
    if (scenario == "advanced-counters") put(live->renderer, 0x1f210, std::uint64_t{49});
    b::EvaluateContractSnapshot input{}; input.summary.complete = true;
    constexpr unsigned keys[]{0, 1, 4, 3};
    for (unsigned i = 0; i < 4; ++i) {
      auto &v = input.resources[keys[i]]; v.value = 0x1000 + i;
      v.value_is_null = false;
      v.present = v.query_attempted = v.callback_completed = true; v.getter_result = 1;
    }
    auto &reset = input.signed_values[static_cast<unsigned>(b::EvaluateSignedKey::reset)];
    reset.value = 1; reset.present = reset.query_attempted = reset.callback_completed = true; reset.getter_result = 1;
    consumer_permit = b::bind_native_depth_evaluate(0x1234, input);
    if (!consumer_permit) std::cerr << "fixture Evaluate frame rejection " << b::native_depth_frame_packet().rejection << '\n';
  }
}
extern "C" __declspec(noinline) A __fastcall mhw_taa_fixture_producer(A arg) {
  (void)arg;
  ++producer_calls;
  if (scenario == "early-preparation")
    mhw_depth_fixture_prepare(at(live->context), at(live->payload), at(live->descriptor));
  if (scenario == "early-thread") {
    std::thread early([] { mhw_depth_fixture_prepare(at(live->context), at(live->payload), at(live->descriptor)); });
    early.join();
  }
  if (scenario == "bad-publication") put(live->descriptor, 16, 4U);
  SetLastError(73);
  return at(live->entries);
}
void prepared_work() {
  if (scenario == "bad-preparation")
    put(live->owners[3], 144, 17U);
  if (scenario == "bad-frame-preparation") put(live->payload, 40, 1.f);
  if (scenario == "advanced-counters") {
    put(live->renderer, 0x1f210, std::uint64_t{17});
    put(live->renderer, 0x1f208, std::uint64_t{28}); put(live->renderer, 0x1f200, 1U);
    live->epoch = 28; put(live->trackers[3], 16, live->epoch);
    // The sorted queue entry is no longer the stable identity.
    put(live->entries, 8, A{}); put(live->descriptor, 8, A{12345});
  }
  mhw_depth_fixture_prepare(at(live->context), at(live->payload),
                            at(live->descriptor));
  if (scenario == "bad-preparation")
    put(live->owners[3], 144, 16U);
  if (scenario == "bad-consumer")
    put(live->owners[3], 96, at(live->backings[0]));
  if (scenario == "bad-frame-consumer") put(live->payload, 44, 1.f);
  mhw_depth_fixture_consume(at(live->payload), 0x1234);
}
void pure_tests() {
  Fixture f;
  auto p = f.producer();
  check(p.valid, "producer snapshot");
  for (unsigned sharpen = 0; sharpen < 2; ++sharpen) {
    put(f.layer, 391, static_cast<unsigned char>(sharpen));
    for (A index = 0; index < 3; ++index) {
      f.output_index = index;
      f.next_index = index;
      put(f.layer, 360 + 8 * static_cast<unsigned>(index), at(f.owners[2]));
      check(f.producer().valid,
            "both branches accept all three history buffers");
    }
  }
  f.output_index = 3;
  f.next_index = 3;
  check(!f.producer().valid, "out-of-range history index rejected");
  f.output_index = 0;
  f.next_index = 1;
  put(f.layer, 391, static_cast<unsigned char>(0));
  const auto producer_reads = p.reads;
  auto s = f.prepared();
  check(s.valid && p.identity == s.identity,
        "preparation matches original producer layout");
  const auto preparation_reads = s.reads;
  check(producer_reads <= 160 && preparation_reads <= 160, "bounded reads");
  for (unsigned i = 1; i <= producer_reads; ++i) {
    f.fail_read = i;
    check(!f.producer().valid, "producer failed read rejected");
  }
  for (unsigned i = 1; i <= preparation_reads; ++i) {
    f.fail_read = i;
    check(!f.prepared().valid, "preparation failed read rejected");
  }
  f.fail_read = 0;
  const auto frame = b::capture_producer_frame(f.base, at(f.args), Fixture::read, &f);
  check(frame.valid && frame.frame == 6 && frame.current_index == 0 && frame.next_index == 1 &&
      frame.jitter_bits[0] == std::bit_cast<unsigned>(0.25f), "frame/history/active jitter source");
  check(frame.reads <= 128, "bounded frame reads");
  for (unsigned i = 1; i <= frame.reads; ++i) {
    f.read_calls = 0; f.fail_read = i;
    check(!b::capture_producer_frame(f.base, at(f.args), Fixture::read, &f).valid, "every failed frame read rejects");
  }
  f.fail_read = 0;
  for (unsigned mode = 1; mode <= 2; ++mode) {
    put(f.renderer, 0x7b424, mode);
    const auto offset = mode == 1 ? 0xe740U : 0xe6a0U;
    put(f.jitter_table, offset + 6 * 8, std::numeric_limits<float>::infinity());
    check(!b::capture_producer_frame(f.base, at(f.args), Fixture::read, &f).valid, "nonfinite jitter rejected");
    put(f.jitter_table, offset + 6 * 8, 0.25f);
    check(b::capture_producer_frame(f.base, at(f.args), Fixture::read, &f).valid, "both jitter modes sampled");
  }
  put(f.renderer, 0x7b424, 1U);
  f.mutate_frame = true; f.frame_reads = 0;
  check(!b::capture_producer_frame(f.base, at(f.args), Fixture::read, &f).valid, "frame changes during producer snapshot reject");
  f.mutate_frame = false;
  check(!b::frame_anchor_matches(f.base, frame, Fixture::read, &f), "resource discovery crossing frame bracket rejected");
  put(f.renderer, 0x1f210, std::uint64_t{6});
  const auto publication = f.publication(frame);
  check(publication.valid && publication.reads <= 128, "bounded publication receipt");
  check(publication.descriptor == at(f.descriptor) && publication.payload == at(f.payload), "independent descriptor and payload allocations");
  for (unsigned n = 1; n <= publication.reads; ++n) {
    f.fail_read = n; check(!f.publication(frame).valid, "every failed publication read rejects");
  }
  f.fail_read = 0;
  for (A address : {at(f.renderer) + 0x710 + 0x2b78, at(f.renderer) + 0x710 + 0x2b80,
                    at(f.entries) + 8, at(f.descriptor) + 32, at(f.payload), at(f.payload) + 44,
                    at(f.renderer) + 0x1f208, at(f.renderer) + 0x1f210}) {
    f.mutate_address = address; f.mutate_visits = 0;
    check(!f.publication(frame).valid, "publication mutable anchors reject");
  }
  f.mutate_address = 0;
  check(!f.publication(frame, at(f.entries) + 16).valid, "return must name last inserted entry");
  const auto reject_word = [&](auto &buffer, unsigned offset, unsigned value, const char *why) {
    unsigned before{}; std::memcpy(&before, buffer.data() + offset, sizeof(before));
    put(buffer, offset, value); check(!f.publication(frame).valid, why); put(buffer, offset, before);
  };
  reject_word(f.descriptor, 0, 22, "wrong opcode");
  reject_word(f.descriptor, 16, 4, "wrong subtype");
  reject_word(f.payload, 60, 1, "reserved payload bits");
  reject_word(f.payload, 40, 0x7f800000, "nonfinite payload reset");
  reject_word(f.payload, 48, 1, "rectangle mismatch");
  reject_word(f.renderer, 0x710 + 0x2b80, 0, "empty publication array");
  reject_word(f.renderer, 0x710 + 0x2b80, 7, "count exceeds capacity");
  reject_word(f.renderer, 0x710 + 0x2b84, 0x100001, "capacity bound");
  reject_word(f.renderer, 0x1f200, 1, "bank generation changed before publication");
  put(f.renderer, 0x710 + 0x2b78, (std::numeric_limits<A>::max)() - 15);
  check(!f.publication(frame).valid, "entry array addition overflow");
  put(f.renderer, 0x710 + 0x2b78, at(f.entries));
  // Arena refills may put descriptor and payload in unrelated allocations.
  auto separate_payload = f.payload;
  put(f.descriptor, 32, at(separate_payload));
  check(f.publication(frame).valid, "payload adjacency is not required");
  put(f.descriptor, 32, at(f.payload));
  for (unsigned bad = 0; bad < 11; ++bad) {
    auto q = publication; auto work = s;
    if (bad == 0) q.payload = 0;
    if (bad == 1) q.descriptor = 0;
    if (bad == 2) ++q.wrapper;
    if (bad == 3) ++q.owners[2];
    if (bad == 4) ++work.identity.textures[3].native;
    if (bad == 5) ++q.rect_bits[0];
    if (bad == 6) ++q.reset_value_bits;
    if (bad == 7) q.valid = false;
    if (bad == 8) ++q.epoch;
    if (bad == 9) q.bank ^= 1;
    if (bad == 10) q.async_mode ^= 1;
    b::FramePacketGate frames; check(frames.queue(frame, p.identity), "receipt reject fixture");
    check(!frames.publish(q, work) && !frames.publish(publication, s), "bad receipt cannot recover or retry");
  }
  const auto stage = b::capture_stage_frame(f.base, at(f.payload), at(f.descriptor), at(f.context), Fixture::read, &f);
  check(stage.valid && stage.epoch == 7 && stage.frame == 6, "epoch retained separately from renderer frame");
  for (unsigned bad = 0; bad < 5; ++bad) {
    b::FramePacketGate frames;
    check(frames.queue(frame, p.identity), "one frame packet queued");
    check(frames.publish(publication, s), "receipt binds original work");
    for (unsigned n = 0; n < 4; ++n) {
      auto changed = stage;
      if (n == 2) {
        if (bad == 1) ++changed.payload;
        if (bad == 2) ++changed.epoch;
        if (bad == 3) ++changed.reset_value_bits;
        if (bad == 4) ++changed.rect_bits[0];
      }
      check(frames.prepare(n, changed) == (!bad || n < 2), "cross-stage frame/payload/epoch mismatch rejected");
    }
    check(frames.consume(stage, 42) == (bad == 0), "only matching frame can reach consumer");
  }
  for (unsigned bad = 0; bad < 5; ++bad) {
    b::FramePacketGate frames; check(frames.queue(frame, p.identity), "contract fixture producer");
    check(frames.publish(publication, s), "contract fixture publication");
    for (unsigned n = 0; n < 4; ++n) check(frames.prepare(n, stage), "contract fixture preparation");
    check(frames.consume(stage, 42), "contract fixture consumer");
    b::EvaluateContractSnapshot input{}; input.summary.complete = true;
    constexpr unsigned keys[]{0, 1, 4, 3};
    for (unsigned i = 0; i < 4; ++i) {
      auto &v = input.resources[keys[i]]; v.value = p.identity.textures[i].native; v.value_is_null = false;
      v.query_attempted = v.callback_completed = v.present = true; v.getter_result = 1;
    }
    auto &reset = input.signed_values[static_cast<unsigned>(b::EvaluateSignedKey::reset)];
    reset.value = 1; reset.getter_result = 1; reset.query_attempted = reset.callback_completed = reset.present = true;
    auto current = stage;
    if (bad == 1) ++input.resources[3].value;
    if (bad == 2) reset.value = 0;
    if (bad == 3) ++current.payload;
    if (bad == 4) input.summary.fault_latched = true;
    check(frames.evaluate(42, current, input) == (bad == 0), "Evaluate binds actual resources, reset and frame");
    check(!frames.evaluate(42, current, input), "second Evaluate binding always rejected");
  }
  {
    b::FramePacketGate frames; check(frames.queue(frame, p.identity), "early preparation fixture");
    check(!frames.prepare(0, stage) && !frames.publish(publication, s), "preparation before receipt permanently rejects");
  }
  check(b::depth_tracking_context_verified(at(f.context), at(f.descriptor),
                                           s.identity.textures[3],
                                           Fixture::read, &f),
        "state-8 tracker admitted");
  for (auto state : {std::uint16_t{4}, std::uint16_t{8}, std::uint16_t{0x800},
                     std::uint16_t{2}}) {
    put(f.trackers[3], 26, state);
    put(f.active, 34, state);
    check(b::depth_tracking_context_verified(at(f.context), at(f.descriptor),
                                             s.identity.textures[3],
                                             Fixture::read, &f) == (state != 2),
          "observed tracker branches only");
  }
  for (unsigned bad = 0; bad < 7; ++bad) {
    b::DepthWorkGate gate;
    check(gate.queue(p), "queue once");
    check(!gate.queue(p), "no second queued item");
    for (unsigned ordinal = 0; ordinal < 4; ++ordinal) {
      constexpr unsigned indices[]{0, 1, 3, 2};
      auto current = s;
      A backing =
          ordinal == 2 ? 0 : s.identity.textures[indices[ordinal]].backing;
      unsigned request = ordinal == 3 ? 0x400 : 4, order = ordinal;
      if (ordinal == 2) {
        if (bad == 1)
          current.identity.wrapper++;
        if (bad == 2)
          backing = 1;
        if (bad == 3)
          request = 8;
        if (bad == 4)
          order = 3;
      }
      const auto selected =
          gate.prepare(order, current, at(f.payload), at(f.descriptor),
                       at(f.context), 1, backing, request, bad != 5);
      if (bad >= 1 && bad <= 5 && ordinal >= 2)
        check(!selected, "bad preparation blocked");
      else
        check(selected == s.identity.textures[indices[ordinal]].backing,
              "correct selected backing");
    }
    auto consume = s;
    if (bad == 6)
      consume.identity.textures[3].backing++;
    const bool allowed = gate.consume(consume, at(f.payload));
    check(allowed == (bad == 0), "consumer gate");
    if (allowed)
      gate.finish();
    check(!gate.queue(p) && !gate.consume(s, at(f.payload)),
          "no reuse after retirement");
  }
  check(b::parse_local_capture_opt_in({b::Mode::capture_one_evaluate, true},
                                      "native-depth-probe\n")
                .mode == b::Mode::native_depth_probe,
        "explicit local mode");
  check(b::evaluate_limit(b::Mode::native_depth_probe) == 1 &&
            b::optimal_settings_call_policy(b::Mode::native_depth_probe, 33) ==
                b::OptimalSettingsCallPolicy::unrecorded,
        "bounded Evaluate with continued fallback sizing");
  const auto generation = b::fallback_generation_token(2);
  check(b::core_create_record_matches({1, 0, 123, 456, generation}, generation,
                                      456, b::Mode::native_depth_probe),
        "probe core record uses fresh generation");
  check(!b::core_create_record_matches({1, 0, 123, 456, generation}, generation,
                                       456, b::Mode::capture_one_evaluate),
        "legacy token contract not widened");
  b::OneShotLifecycle life(b::Mode::native_depth_probe,
                           b::fallback_generation_token(1));
  const auto transition = [&](b::LifecycleOperation op,
                              b::ExportId id = b::ExportId::unknown,
                              std::uint64_t token = 0) {
    constexpr A module = 0x7ffe00000000ULL, rva = 0x1234;
    return life.apply({op,
                       {true, true, true, true, true},
                       {id, module, module + rva, rva, 0x100000, true, true,
                        true, true, true},
                       token});
  };
  check(transition(b::LifecycleOperation::verify).transition_applied,
        "probe verify");
  check(transition(b::LifecycleOperation::initialize, b::ExportId::d3d12_init)
            .transition_applied,
        "probe init");
  const auto create = transition(b::LifecycleOperation::capture_create,
                                 b::ExportId::d3d12_create_feature);
  check(create.issued_token == b::fallback_generation_token(1),
        "probe first token");
  check(transition(b::LifecycleOperation::capture_evaluate,
                   b::ExportId::d3d12_evaluate_feature, create.issued_token)
            .capture_committed,
        "one probe Evaluate admitted");
  check(b::sizing_lifecycle_ready(b::Mode::native_depth_probe, life.state(),
                                  true),
        "sizing after Evaluate");
  check(transition(b::LifecycleOperation::release,
                   b::ExportId::d3d12_release_feature, create.issued_token)
            .transition_applied,
        "probe release");
  for (unsigned i = 2; i <= 20; ++i) {
    check(b::sizing_lifecycle_ready(b::Mode::native_depth_probe, life.state(),
                                    false),
          "sizing between probe generations");
    const auto next = transition(b::LifecycleOperation::capture_create,
                                 b::ExportId::d3d12_create_feature);
    check(next.issued_token == b::fallback_generation_token(i),
          "new fallback generation after probe spent");
    check(transition(b::LifecycleOperation::release,
                     b::ExportId::d3d12_release_feature, next.issued_token)
              .transition_applied,
          "continued fallback release past ledger");
  }
  const auto extra = transition(b::LifecycleOperation::capture_create,
                                b::ExportId::d3d12_create_feature);
  check(!transition(b::LifecycleOperation::capture_evaluate,
                    b::ExportId::d3d12_evaluate_feature, extra.issued_token)
                .capture_committed &&
            life.state() == b::LifecycleState::inert,
        "no replenished Evaluate budget across generations");
}
int main(int argc, char **argv) {
  try {
    check(argc == 2 || argc == 3, "scenario and optional owned packet output required");
    scenario = argv[1];
    check(scenario == "success" || scenario == "bad-producer" ||
              scenario == "bad-preparation" || scenario == "bad-consumer" ||
              scenario == "bad-frame-preparation" || scenario == "bad-frame-consumer" ||
              scenario == "advanced-counters" || scenario == "bad-publication" ||
              scenario == "early-preparation" || scenario == "early-thread" || scenario == "duplicate-publication" || scenario == "missing-projection" || scenario == "jitter-sequence",
          "known scenario");
    pure_tests();
    Fixture f;
    if (scenario == "missing-projection") put(f.renderer, 0x710 + 0x718, A{0});
    live = &f;
    const auto owners_before = f.owners;
    check(b::install_owned_depth_probe(
              f.base, reinterpret_cast<A>(&mhw_depth_fixture_helper),
              reinterpret_cast<A>(&mhw_depth_fixture_consumer),
              {reinterpret_cast<A>(&mhw_depth_fixture_color_return),
               reinterpret_cast<A>(&mhw_depth_fixture_motion_return),
               reinterpret_cast<A>(&mhw_depth_fixture_depth_return),
               reinterpret_cast<A>(&mhw_depth_fixture_output_return)},
              reinterpret_cast<A>(&mhw_depth_fixture_consumer_return)),
          "install real preparation and consumer hooks");
    check(b::install_owned_taa_fallback(
              reinterpret_cast<A>(&mhw_taa_fixture_producer),
              reinterpret_cast<A>(&mhw_taa_fixture_return), true),
          "install producer hook");
    if (scenario == "bad-producer")
      put(f.owners[3], 144, 17U);
    SetLastError(51);
    const auto result = mhw_taa_fixture_call(at(f.args));
    if (scenario == "bad-producer")
      check(result == 0 && GetLastError() == 51 && producer_calls == 0 &&
                helper_calls == 0 && consumer_calls == 0,
            "bad producer fully suppressed");
    else
      check(result == at(f.entries) && GetLastError() == 73 &&
                producer_calls == 1,
            "original producer and result retained once");
    if (scenario == "duplicate-publication") b::publish_native_depth_producer(result);
    if (scenario != "bad-producer") prepared_work();
    const bool success = scenario == "success" || scenario == "advanced-counters" || scenario == "missing-projection" || scenario == "jitter-sequence";
    if (success) {
      check(helper_calls == 4 && consumer_calls == 1 && consumer_permit,
            "ordered preparation and scoped consumer");
      check(helper_backings[0] == at(f.backings[0]) &&
                helper_backings[1] == at(f.backings[1]) &&
                helper_backings[2] == at(f.backings[3]) &&
                helper_backings[3] == at(f.backings[2]),
            "only null depth replaced");
      check(f.owners == owners_before, "no owner field alias/write");
      const auto packet = b::native_depth_frame_packet();
      check(packet.producer.projection.attempted &&
          packet.producer.projection.valid == (scenario != "missing-projection") &&
          packet.producer.projection.matrix_relation == (scenario != "missing-projection"),
          "projection capture remains observational and never relabels later frames");
      check(packet.publication_bound && packet.evaluate_bound && packet.producer.frame == 6 &&
          packet.publication.frame == 6 && packet.evaluate_frame == (scenario == "advanced-counters" ? 49U : 6U),
          "immutable source receipt survives later counter and queue-entry changes");
    }
    if (scenario == "bad-preparation" || scenario == "bad-frame-preparation" ||
        scenario == "bad-publication" || scenario == "early-preparation" || scenario == "early-thread" || scenario == "duplicate-publication")
      check(helper_calls == 0 && consumer_calls == 0,
            "invalid work skips every helper and consumer even after metadata "
            "recovers");
    if (scenario == "bad-consumer" || scenario == "bad-frame-consumer")
      check(helper_calls == 4 && consumer_calls == 0,
            "changed consumer resource rejected before execution");
    const auto depth_stats = b::native_depth_probe_stats();
    if (success)
      check(depth_stats.phase ==
                    static_cast<unsigned>(b::DepthWorkPhase::complete) &&
                depth_stats.depth_corrected == 1 &&
                depth_stats.consumer_completed == 1 &&
                !depth_stats.rejection_stage,
            "completed diagnostic telemetry");
    else
      check(depth_stats.phase ==
                    static_cast<unsigned>(b::DepthWorkPhase::rejected) &&
                depth_stats.rejection_stage == (scenario == "bad-producer" ? 1U
                                                : (scenario == "bad-publication" || scenario == "duplicate-publication") ? 4U
                                                : (scenario == "early-preparation" || scenario == "early-thread") ? 5U
                                                : (scenario == "bad-preparation" || scenario == "bad-frame-preparation")
                                                    ? 10U
                                                    : 20U),
            "first rejection preserved");
    if (scenario == "jitter-sequence") {
      const auto initial = b::native_jitter_sequence_state(GetTickCount64());
      check(initial.count == 1 && initial.initial == 1, "existing hook starts first CPU sample");
      for (unsigned i = 1; i < 32; ++i) {
        const std::uint64_t n = 6 + i;
        put(f.renderer, 0x1f210, n); put(f.renderer, 0x1f208, n);
        put(f.projection_manager, 0x40, static_cast<unsigned>(n));
        f.output_index = n % 3; f.next_index = (n + 1) % 3;
        if (i == 16) {
          b::observe_native_jitter_sequence(at(f.args), true, initial.started + 1600);
          check(b::native_jitter_sequence_state(initial.started + 1600).count == 16, "scene waits for trigger");
          check(b::arm_native_jitter_scene(initial.started + 1600), "one explicit scene trigger");
        }
        if (i >= 24) { f.wrapper = 0; put(f.renderer, 0xa5, static_cast<unsigned char>(0)); }
        b::observe_native_jitter_sequence(at(f.args), i < 24, initial.started + i * 100);
      }
      const auto full = b::native_jitter_sequence_state(initial.started + 3200);
      check(full.count == 32 && full.initial == 16 && full.scene == 16 && full.stop == 1, "both phases bounded");
      b::JitterSample first, last;
      check(b::native_jitter_sample(0, first) && b::native_jitter_sample(31, last) && first.valid && last.valid &&
          first.frame.frame == 6 && last.frame.frame == 37 && first.gates.predicate && last.gates.predicate &&
          first.gates.use_dlss == 1 && last.gates.use_dlss == 0, "complete bracketed On and Off CPU snapshots");
      check(producer_calls == 1 && helper_calls == 4 && consumer_calls == 1, "sequence never admits additional GPU work");
      if (argc == 3) {
        std::ofstream sequence(std::string(argv[2]) + ".sequence.log", std::ios::binary);
        sequence << "trace.start version=0.15.0 owned-fixture=true\n";
        for (unsigned i = 0; i < 32; ++i) {
          b::JitterSample sample; check(b::native_jitter_sample(i, sample), "published sequence slot");
          std::array<char, 8192> text{}; const auto length = b::format_jitter_sample_json(text, sample);
          check(length != 0, "sequence JSON fits bounded worker buffer");
          sequence << "bridge.jitter-sample "; sequence.write(text.data(), length); sequence << '\n';
        }
        sequence << "bridge.jitter-sequence-state {\"schema\":2,\"producer\":1500,\"count\":32,\"initial\":16,\"scene\":16,\"stop\":1,\"started\":" << initial.started << ",\"armed\":" << full.armed << "}\n";
        check(sequence.good(), "owned sequence JSON persisted");
      }
    }
    const auto calls_before = producer_calls;
    SetLastError(52);
    check(mhw_taa_fixture_call(at(f.args)) == 0 && GetLastError() == 52 &&
              producer_calls == calls_before,
          "permanent native TAA after one attempt");
    check(!b::native_depth_evaluate_permitted(),
          "no leaked Evaluate permission");
    const auto old_helpers = helper_calls, old_consumers = consumer_calls;
    check(mhw_depth_fixture_helper(0, 100, 4) == 117 &&
              helper_calls == old_helpers + 1,
          "unrelated helper caller transparent");
    mhw_depth_fixture_consumer(0, 0);
    check(consumer_calls == old_consumers + 1 && !consumer_permit,
          "unrelated consumer caller transparent");
    check(b::remove_owned_taa_fallback() && b::remove_owned_depth_probe(),
          "owned hooks removed");
    if (argc == 3) {
      std::array<char, 8192> json{};
      const auto length = b::format_frame_packet_json(json, b::native_depth_frame_packet());
      check(length != 0, "frame receipt JSON fits bounded worker buffer");
      std::ofstream out(argv[2], std::ios::binary); out.write(json.data(), length); out << '\n';
      check(out.good(), "write owned receipt fixture");
    }
    std::cout << "PASS: " << scenario
              << ", bounded read failures, work lifetime, real "
                 "producer/preparation/consumer hooks and fallback\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

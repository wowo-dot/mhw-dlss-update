// TAA's requests: the producer's packet with its projection, depth and
// resource pins (hook 0), the depth copy packets (hook 2), and their consumers
// (hooks 1 and 3) that claim them for the dispatch.
#include "runtime_state.hpp"

namespace mhw::continuous_dlaa::detail {
bool taa_before(const NativeFrame &f) {
    if (!accepting.load() || f.call.caller != cfg.image + tb::caller_rva) return false;
    publishing.fetch_add(1);
    producer = {};
    auto &s = producer.source;
    unsigned reason{};
    std::array<std::uint64_t, 4> values{};
    const auto gate = [&](unsigned code, bool passed) {
        if (!passed) reason = code;
        return passed;
    };
    bool ok = gate(1, !f.call.stack[0] && !static_cast<unsigned char>(f.call.stack[1])) &&
              gate(2, tb::snapshot(reader, cfg, f.layer, f.call.gpr[1], s)) &&
              gate(3, tb::scene_depth(reader, s, producer.depth)) &&
              gate(4, tb::shader_recipe(reader, s, producer.shader)) &&
              gate(5, reader.get(s.context + 0x2b80, producer_count) && producer_count < 65535 &&
                          reader.get(s.context + 0x2b78, producer_array) && producer_array);
    if (ok) {
        Lock lock;
        producer.projection = projections[s.frame % projections.size()];
        const auto &a = producer.projection.source.anchor;
        values = {a.frame, s.frame, a.view, s.view};
        ok = gate(6, producer.projection.valid) && gate(7, a.frame == s.frame && a.epoch == s.epoch) &&
             gate(8, a.view == s.view && a.context == s.context) &&
             gate(9, a.bank == s.bank && a.async_mode == s.async);
    }
    render_resolution::InputStamp scale;
    if (ok) {
        ok = gate(10, render_resolution::input_stamp(s.view, scale) && scale.frame == s.frame &&
                          scale.render_width == render_width() && scale.render_height == render_height());
        values = {scale.frame, s.frame, scale.render_width, scale.render_height};
    }
    if (ok) {
        Lock lock;
        for (const auto &d : depths)
            if (d.live && d.stamp.frame == s.frame && d.stamp.epoch == s.epoch && d.stamp.view == s.view &&
                d.stamp.bank == s.bank && d.stamp.async == s.async && d.destination == producer.depth.texture &&
                d.sequence > producer.depth_sequence) {
                producer.depth_sequence = d.sequence;
                producer.depth_object = d.object;
                producer.depth_packet = d.packet;
            }
    }
    dlss_sdk::PixelJitter mapped;
    const auto &projection_bits = producer.projection.source.projection;
    if (ok) {
        ok = gate(11, dlss_sdk::projection_jitter(scaled_geometry,
                                                  {std::bit_cast<float>(projection_bits.jitter_bits[0]),
                                                   std::bit_cast<float>(projection_bits.jitter_bits[1])},
                                                  std::bit_cast<float>(projection_bits.viewport_bits[0]),
                                                  std::bit_cast<float>(projection_bits.viewport_bits[1]), mapped));
        values = {projection_bits.jitter_bits[0], projection_bits.jitter_bits[1], projection_bits.viewport_bits[0],
                  projection_bits.viewport_bits[1]};
    }
    if (ok) {
        values = {s.rectangle[0], s.rectangle[1], s.rectangle[2], s.rectangle[3]};
        ok = gate(12, s.rectangle == std::array<unsigned, 4>{0, 0, render_width(), render_height()});
    }
    if (ok) {
        values = {s.inputs[0].format, s.inputs[5].format, s.inputs[4].format, producer.depth.texture.format};
        ok = gate(13, pin(s.inputs[0].resource, 26, producer.resources[0]) &&
                          pin(s.inputs[5].resource, 42, producer.resources[1]) &&
                          pin(s.inputs[4].resource, 26, producer.resources[2], true) &&
                          pin(producer.depth.texture.resource, 41, producer.resources[3]));
    }
    if (!ok) {
        ++producer_refusals[reason < producer_refusals.size() ? reason : 0];
        {
            Lock lock;
            last_producer_refusal = {s.frame, reason, values};
        }
        producer.resources = {};
        ++skips;
        publishing.fetch_sub(1);
        return false;
    }
    producer.projection.history.depth = producer.depth.texture.resource;
    return true;
}
RequestIdentity identity(const Taa &t) {
    const auto &s = t.source;
    return {s.frame,
            s.epoch,
            t.sequence,
            t.projection.history.microseconds,
            t.packet.object,
            t.packet.state,
            s.view,
            s.context,
            s.layer,
            t.depth.texture.resource,
            s.bank,
            s.async,
            t.bypasses,
            t.last_bypass_tick,
            t.last_bypass_caller,
            t.last_bypass_publishers};
}
// Called under mutex after the original producer and all existing validations.
// Preserve fail(6), the original slot and publication sequence on collision.
void publish_taa() {
    auto &m = request_telemetry;
    ++m.publication_attempts;
    const auto index = producer.source.frame % requests.size();
    auto &slot = requests[index];
    if (slot.live) {
        if (!m.collisions) {
            auto &c = m.first_collision;
            c.tick = GetTickCount64();
            c.publication_attempt = m.publication_attempts;
            c.slot = static_cast<unsigned>(index);
            for (const auto &t : requests)
                if (t.live) ++c.live;
            c.publishers = publishing.load();
            c.pending_callbacks = pending.load();
            c.old_request = identity(slot);
            c.new_request = identity(producer);
            c.lease_list = reinterpret_cast<std::uintptr_t>(lease.list);
            c.lease_generation = lease.generation;
            c.lease_frame = lease.frame;
            c.lease_depth = lease.depth;
            c.lease_evaluated = lease.evaluated;
        }
        ++m.collisions;
        fail(6);
    } else {
        producer.sequence = ++sequence;
        producer.live = true;
        slot = producer;
        ++m.published;
    }
}
void taa_after(bool admitted, bool returned) {
    if (!admitted) return;
    tb::Snapshot again;
    tb::SceneDepth depth;
    tb::ShaderRecipe shader;
    unsigned count{};
    std::uintptr_t array{}, object{};
    const auto &s = producer.source;
    const bool ok =
        returned && tb::snapshot(reader, cfg, s.layer, s.context, again) && s == again &&
        tb::scene_depth(reader, s, depth) && depth == producer.depth && tb::shader_recipe(reader, s, shader) &&
        shader == producer.shader && reader.get(s.context + 0x2b80, count) && count == producer_count + 1 &&
        reader.get(s.context + 0x2b78, array) && array && reader.get(array + 16ULL * (count - 1) + 8, object) &&
        tb::packet(reader, object, producer.packet) &&
        tb::packet_matches(s, producer.packet, (render_width() + 15) / 16, (render_height() + 15) / 16, 1);
    if (ok) {
        Lock lock;
        publish_taa();
        Depth selected;
        selected.stamp.frame = s.frame;
        selected.stamp.epoch = s.epoch;
        selected.sequence = producer.depth_sequence;
        note_depth(2, producer.depth_sequence ? 0U : 1U, selected,
                   {producer.sequence, producer.packet.object, s.context, s.view, producer.depth.texture.resource});
        // A prepared full-output frame must have a qualified color resolve
        // before downstream CPU workers are allowed to commit their geometry.
        std::array<std::byte, native_taa_shader_bytes> native_bytes{};
        const auto variant = native_shader_index(producer.shader.technique, producer.shader.size, include_menu_shader);
        const bool color_contract =
            single_native_output(s, producer.shader, include_menu_shader) &&
            pb::copy_memory(producer.shader.bytecode, native_bytes.data(), producer.shader.size) &&
            preparation_shader_cache.matches(
                variant, std::span<const std::byte>(native_bytes).first(producer.shader.size),
                [](std::span<const std::byte> data) {
                    return pb::bytes_sha256(data.data(), static_cast<unsigned>(data.size()));
                });
        if (accepting.load() && color_contract) sr::publish(producer.source, producer.packet);
    } else
        fail(7);
    producer.resources = {};
    publishing.fetch_sub(1);
}
bool depth_before(const NativeFrame &f) {
    if (!accepting.load() || f.call.gpr[1] != cfg.renderer + 0x710 || static_cast<unsigned>(f.call.stack[0]) != 8)
        return false;
    depth_producer = {};
    auto &d = depth_producer;
    std::uintptr_t destination{}, attachment{}, source{};
    unsigned reason{};
    const auto gate = [&](unsigned code, bool ok) {
        if (!ok) reason = code;
        return ok;
    };
    const bool ok = gate(1, dr::stamp(reader, cfg, d.stamp)) &&
                    gate(2, reader.get(d.stamp.container + 0xa0, destination) && destination == f.call.gpr[4]) &&
                    gate(3, reader.get(d.stamp.context + 0x50, attachment) && attachment &&
                                reader.get(attachment + 0x58, source)) &&
                    gate(4, tb::read_owner(reader, source, d.source)) &&
                    gate(5, tb::read_owner(reader, destination, d.destination)) &&
                    gate(6, reader.get(d.stamp.context + 0x2b80, depth_producer_count) && depth_producer_count < 65535);
    unsigned flags{};
    const bool flags_read = reader.get(cfg.renderer + 0x710 + 0x1ea74, flags);
    note_depth(0, reason, d,
               {f.call.gpr[1], f.call.gpr[4], destination, attachment, source, d.stamp.container, flags_read, flags,
                depth_producer_count});
    return ok;
}
void depth_after(bool admitted, bool returned) {
    if (!admitted) return;
    auto &d = depth_producer;
    unsigned count{};
    std::uintptr_t array{};
    dr::Stamp again;
    if (!returned || !dr::stamp(reader, cfg, again) || again != d.stamp ||
        !reader.get(d.stamp.context + 0x2b80, count) || count <= depth_producer_count ||
        count - depth_producer_count > 64 || !reader.get(d.stamp.context + 0x2b78, array) || !array ||
        !reader.get(array + 16ULL * (count - 1) + 8, d.object) || !dr::packet(reader, d.object, d.packet) ||
        d.packet.source != d.source.object || d.packet.destination != d.destination.object) {
        fail(8);
        return;
    }
    Lock lock;
    d.sequence = ++sequence;
    d.live = true;
    depths[d.sequence % depths.size()] = d;
    note_depth(1, 0, d,
               {d.object, d.packet.nested, d.source.resource, d.destination.resource, d.packet.rectangle[0],
                d.packet.rectangle[1], d.packet.rectangle[2], d.packet.rectangle[3], d.packet.rectangle[4],
                d.packet.rectangle[5]});
}
void begin_forwarded(const ConsumerSample &sample, ForwardedRequest &token) {
    auto &m = forwarded_telemetry;
    ++m.candidates;
    if (!sample.matches) {
        ++m.unmatched;
        return;
    }
    if (sample.matches != 1) {
        ++m.ambiguous;
        return;
    }
    const auto index = sample.newest.frame % requests.size();
    auto &t = requests[index];
    tb::Packet a, b;
    if (!t.live || t.forwarding || !t.sequence || t.sequence != sample.newest.sequence ||
        !tb::packet(reader, sample.packet, a) || !tb::packet(reader, sample.packet, b) || a != b || a != t.packet ||
        !tb::packet_matches(t.source, a, (render_width() + 15) / 16, (render_height() + 15) / 16, 1)) {
        ++m.invalid;
        return;
    }
    t.forwarding = true;
    token = {t.sequence, static_cast<unsigned>(index), a};
    native_forwarding = true;
    ++m.started;
    ++m.pending;
}
void finish_forwarded(ForwardedRequest &token, bool returned) {
    if (!token.sequence) return;
    Lock lock;
    auto &m = forwarded_telemetry;
    auto &t = requests[token.slot];
    tb::Packet a, b;
    const bool same = t.live && t.forwarding && t.sequence == token.sequence && t.packet == token.packet;
    const bool stable = returned && same && tb::packet(reader, token.packet.object, a) &&
                        tb::packet(reader, token.packet.object, b) && a == b && a == token.packet;
    --m.pending;
    native_forwarding = false;
    token.sequence = 0;
    if (!stable) {
        ++m.failed;
        if (same) t.forwarding = false;
        fail(returned ? 32 : 10);
        return;
    }
    const auto receipt = identity(t);
    if (!m.retired) m.first_retired = receipt;
    m.last_retired = receipt;
    ++m.retired;
    t.live = false;
    t.forwarding = false;
    t.resources = {};
}
bool consumer_before(const NativeFrame &f, bool depth, ForwardedRequest *forwarded) {
    const bool prepared_output = !depth && sr::owns_packet(f.call.gpr[3]);
    if (!accepting.load() && !prepared_output) return false;
    if (current_kind || native_forwarding) {
        Lock lock;
        auto &m = request_telemetry.consumers[depth ? 1 : 0];
        ++m.calls;
        ++m.reentrant;
        fail(9);
        return false;
    }
    const auto publishers = publishing.load();
    if (publishers && !prepared_output) {
        Lock lock;
        auto &m = request_telemetry.consumers[depth ? 1 : 0];
        ++m.calls;
        ConsumerSample sample;
        sample.ordinal = m.calls;
        sample.tick = GetTickCount64();
        sample.packet = f.call.gpr[3];
        sample.caller = f.call.caller;
        sample.publishers = publishers;
        if (depth) {
            for (const auto &d : depths)
                if (d.live) {
                    ++sample.live;
                    if (d.object == sample.packet) {
                        ++sample.matches;
                        if (d.sequence > sample.newest.sequence) {
                            sample.newest.frame = d.stamp.frame;
                            sample.newest.epoch = d.stamp.epoch;
                            sample.newest.sequence = d.sequence;
                            sample.newest.packet = d.object;
                            sample.newest.view = d.stamp.view;
                            sample.newest.context = d.stamp.context;
                        }
                    }
                }
        } else
            for (auto &t : requests)
                if (t.live) {
                    ++sample.live;
                    if (t.packet.object == sample.packet) {
                        ++sample.matches;
                        ++t.bypasses;
                        t.last_bypass_tick = sample.tick;
                        t.last_bypass_caller = sample.caller;
                        t.last_bypass_publishers = publishers;
                        if (t.sequence > sample.newest.sequence) sample.newest = identity(t);
                    }
                }
        if (!m.blocked) m.first_blocked = sample;
        m.last_blocked = sample;
        ++m.blocked;
        if (depth && sample.newest.frame) {
            Depth d;
            d.stamp.frame = sample.newest.frame;
            d.stamp.epoch = sample.newest.epoch;
            d.sequence = sample.newest.sequence;
            note_depth(3, 4, d, {sample.packet, sample.caller, publishers, sample.matches});
        }
        if (!depth && forwarded) begin_forwarded(sample, *forwarded);
        return false;
    }
    bool found = false;
    {
        Lock lock;
        auto &m = request_telemetry.consumers[depth ? 1 : 0];
        ++m.calls;
        ConsumerSample sample;
        sample.ordinal = m.calls;
        sample.packet = f.call.gpr[3];
        sample.caller = f.call.caller;
        if (depth) {
            Depth *latest{};
            for (auto &d : depths)
                if (d.live) {
                    ++sample.live;
                    if (d.object == f.call.gpr[3]) {
                        ++sample.matches;
                        if (!latest || d.sequence > latest->sequence) latest = &d;
                    }
                }
            if (latest) {
                sample.newest.frame = latest->stamp.frame;
                sample.newest.epoch = latest->stamp.epoch;
                sample.newest.sequence = latest->sequence;
                sample.newest.packet = latest->object;
                sample.newest.view = latest->stamp.view;
                sample.newest.context = latest->stamp.context;
            }
            if (latest) {
                depth_consumer = *latest;
                latest->live = false;
                dr::CopyPacket p;
                found = dr::packet(reader, f.call.gpr[3], p) && p == depth_consumer.packet;
                if (!found) note_depth(3, 1, depth_consumer, {f.call.gpr[3], f.call.caller});
            }
        } else {
            Taa *latest{};
            for (auto &t : requests)
                if (t.live) {
                    ++sample.live;
                    if (t.packet.object == f.call.gpr[3]) {
                        ++sample.matches;
                        if (!latest || t.sequence > latest->sequence) latest = &t;
                    }
                }
            if (latest) sample.newest = identity(*latest);
            if (latest && latest->forwarding) {
                ++m.reentrant;
                fail(9);
                return false;
            }
            if (latest) {
                consumer = *latest;
                latest->resources = {};
                latest->live = false;
                tb::Packet p;
                found = tb::packet(reader, f.call.gpr[3], p) && p == consumer.packet;
            }
        }
        if (sample.matches) {
            ++m.matched;
            if (!found) ++m.changed;
            if (sample.matches > 1) {
                sample.tick = GetTickCount64();
                if (!m.multiple) m.first_multiple = sample;
                m.last_multiple = sample;
                ++m.multiple;
            }
        } else {
            sample.tick = GetTickCount64();
            if (!m.unmatched) m.first_unmatched = sample;
            m.last_unmatched = sample;
            ++m.unmatched;
        }
    }
    if (!found) return false;
    std::uintptr_t backend{}, command{};
    if (!f.call.gpr[1] || !f.call.gpr[2] || !reader.get(f.call.gpr[2] + 8, backend) || backend != cfg.backend ||
        !reader.get(f.call.gpr[1] + 8, current_recording) || !current_recording ||
        !reader.get(current_recording + 0x20, command) || !command) {
        if (depth)
            note_depth(3, 2, depth_consumer, {f.call.gpr[1], f.call.gpr[2], backend, current_recording, command});
        return false;
    }
    current_list = reinterpret_cast<ID3D12GraphicsCommandList *>(command);
    {
        Lock lock;
        if (!find(current_list)) {
            if (depth) note_depth(3, 3, depth_consumer, {command, current_recording});
            return false;
        }
    }
    if (depth) note_depth(3, 0, depth_consumer, {command, current_recording});
    current_kind = depth ? 2 : 1;
    barrier_count = dispatch_count = copy_count = 0;
    return true;
}
void consumer_after(bool admitted, bool returned) {
    if (admitted) {
        if (!returned) fail(10);
        current_kind = 0;
        current_list = nullptr;
    }
    consumer.resources = {};
}
} // namespace mhw::continuous_dlaa::detail

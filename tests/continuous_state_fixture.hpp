#pragma once
void state_trace_fixture() {
    constexpr std::uintptr_t color = 0x1000, output = 0x2000, motion = 0x3000, other = 0x4000;
    const auto transition = [](std::uintptr_t r, unsigned a, unsigned b, unsigned sub = UINT_MAX, unsigned flags = 0) {
        D3D12_RESOURCE_BARRIER v{};
        v.Transition = {reinterpret_cast<ID3D12Resource *>(r), sub, static_cast<D3D12_RESOURCE_STATES>(a),
                        static_cast<D3D12_RESOURCE_STATES>(b)};
        v.Flags = static_cast<D3D12_RESOURCE_BARRIER_FLAGS>(flags);
        return v;
    };
    const auto uav = [](std::uintptr_t r) {
        D3D12_RESOURCE_BARRIER v{};
        v.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        v.UAV.pResource = reinterpret_cast<ID3D12Resource *>(r);
        return v;
    };
    const auto alias = [](std::uintptr_t a, std::uintptr_t b) {
        D3D12_RESOURCE_BARRIER v{};
        v.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        v.Aliasing = {reinterpret_cast<ID3D12Resource *>(a), reinterpret_cast<ID3D12Resource *>(b)};
        return v;
    };
    const auto verify = [&](std::initializer_list<D3D12_RESOURCE_BARRIER> values, unsigned result) {
        cd::barrier_count = static_cast<unsigned>(values.size());
        std::copy(values.begin(), values.end(), cd::barriers.begin());
        const auto proof = cd::explain_state(std::span(cd::barriers).first(cd::barrier_count), color, 0x40);
        check(proof.result == result, "state explanation reason");
        check(cd::state_of(color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) == (proof.result == 0),
              "admission parity");
    };
    verify({}, 1);
    verify({uav(color)}, 1); // A UAV barrier does not establish a state.
    verify({transition(color, 4, 0x40)}, 0);
    verify({transition(color, 4, 0x40), transition(color, 0x40, 0x800), transition(color, 0x800, 0x40)}, 0);
    verify({transition(other, 4, 0x40), uav(color)}, 1);
    verify({transition(color, 4, 0xc0)}, 6); // Do not broaden accepted read states.
    verify({transition(color, 4, 0x40), transition(color, 8, 0x40)}, 5);
    verify({transition(color, 4, 0x40, 0)}, 4);
    verify({transition(color, 4, 0x40, UINT_MAX, 1)}, 3);
    verify({transition(color, 4, 0x40), alias(color, other)}, 2);
    verify({alias(0, other), transition(color, 4, 0x40)}, 2);
    verify({alias(other, output), transition(color, 4, 0x40)}, 0);
    unsigned random = 17;
    const auto next = [&] {
        random = random * 1664525U + 1013904223U;
        return random;
    };
    for (unsigned trial = 0; trial < 4096; ++trial) {
        cd::barrier_count = 1 + next() % 8;
        for (unsigned i = 0; i < cd::barrier_count; ++i) {
            const auto kind = next() % 5;
            const auto r = (next() % 3) ? color : other;
            cd::barriers[i] =
                kind == 0   ? uav(r)
                : kind == 1 ? alias(r, next() % 2 ? 0 : other)
                            : transition(r, next() % 256, next() % 256, next() % 4 ? UINT_MAX : 0, next() % 8 ? 0 : 1);
        }
        const auto want = next() % 256;
        const auto p = cd::explain_state(std::span(cd::barriers).first(cd::barrier_count), color, want);
        check(cd::state_of(color, static_cast<D3D12_RESOURCE_STATES>(want)) == (p.result == 0), "4096 trace parity");
    }
    Block packet(0x30), node(0x28), backing(0x28);
    put(packet.p + 8, node.p);
    put(node.p + 8, backing.p);
    put(backing.p + 0x20, output);
    put(node.p + 0x18, std::uint16_t{0x400});
    put(node.p + 0x1a, std::uint16_t{0x400});
    tb::Snapshot source;
    source.frame = 7;
    source.epoch = 9;
    for (unsigned i = 0; i < 7; ++i) {
        auto &o = source.inputs[i];
        o.object = 0x5000 + 256 * i;
        o.backing = 0x6000 + 256 * i;
        o.resource = other;
        o.width = 17;
        o.height = 13;
        o.gpu_format = 26;
    }
    source.inputs[0].resource = color;
    source.inputs[4].resource = output;
    source.inputs[5].resource = motion;
    source.inputs[5].gpu_format = 42;
    std::array<D3D12_RESOURCE_BARRIER, 3> valid{transition(color, 4, 0x40), uav(output), transition(motion, 4, 0xc0)};
    auto trace_storage = std::make_unique<cd::StateTrace>();
    auto &trace = *trace_storage;
    for (unsigned i = 0; i < 20; ++i) {
        source.frame = 7 + i;
        trace.capture(cd::reader, source, 1 + i, reinterpret_cast<ID3D12GraphicsCommandList *>(0x7000), 1 + i, packet.p,
                      100 + i, false, valid);
    }
    std::ostringstream text;
    text << std::boolalpha;
    trace.write(text);
    check(text.str().size() < 60000, "bounded trace after many failures");
    std::cout << "{\"case\":\"missing-transition\",\"state_trace\":" << text.str() << "}\n";
    // Malformed native diagnostic data cannot change runtime health/admission.
    put(node.p, node.p);
    auto cyclic_storage = std::make_unique<cd::StateTrace>();
    auto &cyclic = *cyclic_storage;
    cyclic.capture(cd::reader, source, 1, reinterpret_cast<ID3D12GraphicsCommandList *>(0x7000), 1, packet.p, 100,
                   false, valid);
    std::cout << std::boolalpha << "{\"case\":\"cyclic-native\",\"state_trace\":";
    cyclic.write(std::cout);
    std::cout << "}\n";
    auto capped_storage = std::make_unique<cd::StateTrace>();
    auto &capped = *capped_storage;
    std::array<D3D12_RESOURCE_BARRIER, 32> lots;
    lots.fill(uav(output));
    capped.capture(cd::reader, source, 1, reinterpret_cast<ID3D12GraphicsCommandList *>(0x7000), 1, packet.p, 100,
                   false, lots);
    std::cout << "{\"case\":\"capped-barriers\",\"state_trace\":";
    capped.write(std::cout);
    std::cout << "}\n";
    put(node.p, std::uintptr_t{});
    valid = {transition(color, 4, 0x40), transition(output, 0x40, 8), uav(motion)};
    auto temporal_storage = std::make_unique<cd::StateTrace>();
    auto &temporal = *temporal_storage;
    temporal.capture(cd::reader, source, 1, reinterpret_cast<ID3D12GraphicsCommandList *>(0x7000), 1, packet.p, 100,
                     true, valid);
    std::cout << "{\"case\":\"motion-state\",\"state_trace\":";
    temporal.write(std::cout);
    std::cout << "}\n";
    valid[0] = alias(0, other);
    auto aliased_storage = std::make_unique<cd::StateTrace>();
    auto &aliased = *aliased_storage;
    aliased.capture(cd::reader, source, 1, reinterpret_cast<ID3D12GraphicsCommandList *>(0x7000), 1, packet.p, 100,
                    false, valid);
    std::cout << "{\"case\":\"aliasing\",\"state_trace\":";
    aliased.write(std::cout);
    std::cout << "}\n";
    check(cd::healthy(), "diagnostic reads do not fault renderer");
}

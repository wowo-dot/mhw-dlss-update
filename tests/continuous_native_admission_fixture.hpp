#pragma once
void native_admission_fixture() {
    Block renderer(0x7af40), schedule(16 * 8), packets(0x30 * 8), owners(0x100 * 3), backings(0x80 * 4),
        trackers(0x38 * 3), spans(0x28 * 7), nodes(0x20 * 5);
    tb::Snapshot source;
    source.context = renderer.p + 0x710;
    source.frame = source.epoch = 9;
    std::array<D3D12_RESOURCE_BARRIER, 3> barriers{};
    unsigned count{};
    const auto packet = [&](unsigned i) { return packets.p + 0x30 * i; };
    const auto span = [&](unsigned i) { return spans.p + 0x28 * i; };
    const auto node = [&](unsigned i) { return nodes.p + 0x20 * i; };
    const auto setup = [&](bool local) {
        std::memset(reinterpret_cast<void *>(renderer.p), 0, 0x7af40);
        std::memset(reinterpret_cast<void *>(packets.p), 0, 0x30 * 8);
        std::memset(reinterpret_cast<void *>(trackers.p), 0, 0x38 * 3);
        std::memset(reinterpret_cast<void *>(spans.p), 0, 0x28 * 7);
        std::memset(reinterpret_cast<void *>(nodes.p), 0, 0x20 * 5);
        put(renderer.p + 0x1f208, std::uint64_t{10});
        put(renderer.p + 0x7af18, schedule.p);
        put(renderer.p + 0x7af28, 8U);
        for (unsigned i = 0; i < 8; ++i) {
            put(schedule.p + 16 * i + 8, packet(i));
            put(packet(i), 6U);
        }
        constexpr unsigned roles[]{0, 4, 5};
        for (unsigned i = 0; i < 3; ++i) {
            const auto o = owners.p + 0x100 * i, b = backings.p + 0x80 * i, t = trackers.p + 0x38 * i;
            put(o, std::uintptr_t{1});
            put(o + 0x60, b);
            put(o + 0x88, 17U + (13U << 16));
            put(o + 0x94, 26U);
            put(b + 0x20, std::uintptr_t{0x10000 + 0x1000 * i});
            put(b + 0x70, t);
            check(tb::read_owner(cd::reader, o, source.inputs[roles[i]]), "fixture owner");
            put(t + 0x10, std::uint64_t{10});
            put(t + 0x20, std::uint64_t{10});
            put(t + 0x28, static_cast<unsigned char>(1));
        }
        const auto set_span = [&](unsigned i, unsigned pkt, unsigned last, unsigned before, unsigned after,
                                  std::uintptr_t previous, std::uintptr_t next) {
            put(span(i), packet(pkt));
            put(span(i) + 8, last);
            put(span(i) + 0x10, next);
            put(span(i) + 0x18, previous);
            put(span(i) + 0x20, static_cast<std::uint16_t>(before));
            put(span(i) + 0x22, static_cast<std::uint16_t>(after));
        };
        set_span(0, 0, 0, 8, 0x100, 0, span(1));
        set_span(1, local ? 4 : 2, 4, 0x100, 4, span(0), span(2));
        set_span(2, 6, 6, 4, 0x100, span(1), span(3));
        set_span(3, 7, 7, 0x100, 8, span(2), 0);
        put(trackers.p, span(0));
        put(trackers.p + 8, span(3));
        put(trackers.p + 0x18, std::uint16_t{8});
        put(trackers.p + 0x1a, std::uint16_t{8});
        set_span(4, 2, 2, 0, 0x400, 0, span(5));
        set_span(5, 4, 4, 0x400, 0x400, local ? 0 : span(4), span(6));
        set_span(6, 5, 5, 0x400, 0x20, span(5), 0);
        const auto out = trackers.p + 0x38;
        put(out, span(local ? 5 : 4));
        put(out + 8, span(6));
        put(out + 0x18, static_cast<std::uint16_t>(local ? 0x400 : 0));
        put(out + 0x1a, std::uint16_t{0x20});
        const auto set_node = [&](unsigned i, unsigned role, unsigned use, unsigned before, unsigned after,
                                  std::uintptr_t next) {
            put(node(i), next);
            put(node(i) + 8, backings.p + 0x80 * role);
            put(node(i) + 0x10, span(use));
            put(node(i) + 0x18, static_cast<std::uint16_t>(before));
            put(node(i) + 0x1a, static_cast<std::uint16_t>(after));
        };
        set_node(0, 0, 0, 8, 0x100, 0);
        set_node(1, 0, 1, 0x100, 4, local ? node(3) : node(2));
        set_node(2, 1, 4, 0, 0x400, 0);
        set_node(3, 1, 5, 0x400, 0x400, 0);
        put(packet(0) + 8, node(0));
        put(packet(2) + 8, local ? std::uintptr_t{} : node(1));
        put(packet(4) + 8, node(local ? 1 : 3));
        barriers = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barriers[0].UAV.pResource = reinterpret_cast<ID3D12Resource *>(source.inputs[4].resource);
        barriers[1].Transition = {reinterpret_cast<ID3D12Resource *>(source.inputs[5].resource), UINT_MAX,
                                  D3D12_RESOURCE_STATE_RENDER_TARGET, static_cast<D3D12_RESOURCE_STATES>(0xc0)};
        barriers[2].Transition = {reinterpret_cast<ID3D12Resource *>(source.inputs[0].resource), UINT_MAX,
                                  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
        count = local ? 3 : 2;
    };
    const auto run = [&](const char *name, bool expected, const tb::Reader &reader = cd::reader) {
        auto admission = std::make_unique<cd::NativeStateAdmission>();
        const bool accepted =
            admission->attempt(reader, source, 1, reinterpret_cast<ID3D12GraphicsCommandList *>(0x8000), 3, packet(4),
                               100, std::span(barriers).first(count));
        check(accepted == expected, name);
        std::cout << std::boolalpha << "{\"case\":\"" << name << "\",\"accepted\":" << accepted
                  << ",\"state_admission\":";
        admission->write(std::cout);
        std::cout << "}\n";
    };
    setup(false);
    run("earlier-initialization", true);
    setup(true);
    run("initial-uav", true);
    setup(false);
    put(trackers.p + 0x1a, std::uint16_t{0x800});
    put(trackers.p + 0x38 + 0x1a, std::uint16_t{0x800});
    run("ignore-future-final-state", true);
    setup(false);
    barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    run("flagged-uav", false);
    setup(false);
    barriers[count].Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    barriers[count++].Aliasing = {nullptr, nullptr};
    run("aliasing", false);
    setup(true);
    barriers[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    run("split-color", false);
    setup(true);
    barriers[2].Transition.Subresource = 0;
    run("subresource-color", false);
    setup(true);
    barriers[2].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    run("wrong-color-state", false);
    setup(false);
    put(schedule.p + 8, packet(4));
    run("duplicate-consumer", false);
    setup(false);
    put(renderer.p + 0x1f208, std::uint64_t{11});
    run("wrong-epoch", false);
    setup(false);
    put(trackers.p + 0x10, std::uint64_t{9});
    run("stale-tracker", false);
    setup(false);
    put(span(6) + 0x10, span(4));
    run("cyclic-spans", false);
    setup(false);
    put(span(5) + 0x18, span(3));
    run("broken-backlink", false);
    setup(false);
    put(span(5) + 0x24, static_cast<unsigned char>(1));
    run("unobserved-first-use", false);
    setup(false);
    put(span(1) + 8, 3U);
    run("color-use-ended", false);
    setup(false);
    put(span(5) + 0x22, std::uint16_t{0x20});
    run("wrong-output-use", false);
    setup(false);
    put(node(2) + 0x10, span(6));
    run("wrong-earlier-span", false);
    setup(false);
    put(backings.p + 0x180 + 0x20, source.inputs[4].resource);
    put(node(3) + 8, backings.p + 0x180);
    run("aliased-native-backing", false);
    setup(false);
    put(backings.p + 0x180 + 0x20, source.inputs[0].resource);
    put(node(1) + 8, backings.p + 0x180);
    run("aliased-earlier-backing", false);
    setup(false);
    put(node(3) + 0x10, span(6));
    run("wrong-current-span", false);
    setup(false);
    struct Mutation {
        std::uintptr_t at;
        unsigned calls{};
    };
    Mutation m{renderer.p + 0x1f208};
    const tb::Reader changing{&m, [](void *user, std::uintptr_t at, std::span<std::byte> bytes) {
                                  auto &v = *static_cast<Mutation *>(user);
                                  if (at == v.at && bytes.size() == 8 && ++v.calls >= 3) {
                                      const std::uint64_t changed = 11;
                                      std::memcpy(bytes.data(), &changed, 8);
                                      return true;
                                  }
                                  return cd::reader.read(cd::reader.user, at, bytes);
                              }};
    run("schedule-changed-on-reread", false, changing);
    // Missing proof reads cannot latch a feature failure or mutate rendering.
    setup(false);
    const tb::Reader denied{nullptr, [](void *, std::uintptr_t, std::span<std::byte>) { return false; }};
    run("unreadable", false, denied);
    check(cd::healthy(), "proof failures leave original rendering intact");
}

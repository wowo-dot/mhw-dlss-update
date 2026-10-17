#include "depth_consumer_fixture.hpp"
int main() {
    const auto run = [](const char *name, auto configure, unsigned expected) {
        ConsumerFixture f;
        configure(f);
        auto proof = std::make_unique<cd::DepthConsumerAdmission>();
        const auto state = f.attempt(*proof);
        check(state == expected, name);
        if (state) proof->copied();
        std::cout << std::boolalpha << "{\"case\":\"" << name << "\",\"state\":" << state << ",\"trace\":";
        proof->write(std::cout);
        std::cout << "}\n";
    };
    using F = ConsumerFixture;
    run("render-target", [](F &) {}, 4);
    run("hoisted-outside-prior-32", [](F &f) { f.hoist(); }, 128);
    run("hoisted-at-consumer", [](F &f) { f.hoist(F::consumer_index); }, 128);
    run("combined-read-after-consumer", [](F &f) { f.read_span(59, 60, 12, 3); }, 192);
    run("compute-read-after-consumer", [](F &f) { f.read_span(59, 60, 4, 3); }, 64);
    run("inside-existing-read-span", [](F &f) { f.read_span(40, 60, 8, 40); }, 128);
    run("inside-combined-read-span", [](F &f) { f.read_span(40, 60, 12, 40); }, 192);
    run("read-at-consumer", [](F &f) { f.read_span(56, 60, 12, 56); }, 192);
    run(
        "multiple-read-transitions",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(40, 60, 8, 12, 35);
        },
        192);
    run(
        "future-read-transition-not-executed",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(59, 60, 8, 12, 59);
        },
        128);
    run(
        "future-write-not-executed",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(59, 60, 8, 0x100, 59);
        },
        128);
    run(
        "current-read-ended",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(59, 60, 8, 12, 58);
        },
        128);
    run("missing-begun-read-transition", [](F &f) { f.read_span(40, 60, 8, 59); }, 0);
    run("read-transition-after-first-use", [](F &f) { f.read_span(40, 60, 8, 50); }, 0);
    run(
        "intervening-write",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(40, 60, 8, 0x100, 35);
        },
        0);
    run(
        "hoisted-future-write",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(59, 60, 8, 0x100, 35);
        },
        0);
    run(
        "read-transition-before-prior-last-use",
        [](F &f) {
            f.read_span(10, 45, 8, 10);
            f.third_span(59, 60, 8, 12, 40);
        },
        0);
    run(
        "missing-middle-transition",
        [](F &f) {
            f.read_span(10, 30, 8, 10);
            f.third_span(40, 60, 8, 12, 35);
            f.m.put(F::packets + 10 * 0x100 + 8, std::uintptr_t{});
        },
        0);
    run(
        "overlapping-read-spans",
        [](F &f) {
            f.read_span(10, 45, 8, 10);
            f.third_span(40, 60, 8, 12, 35);
        },
        0);
    run("unknown-read-bits", [](F &f) { f.read_span(40, 60, 0x18, 40); }, 0);
    run("duplicate-reader", [](F &f) { f.m.put(F::schedule + 16 * 4 + 8, F::packets + F::next_index * 0x100); }, 0);
    // Schedule-based oracle: the transition must occur after the writer and
    // no later than its own first use. CPU callback order is irrelevant.
    for (unsigned native : {4U, 8U, 12U})
        for (unsigned first : {10U, 40U, 56U, 59U})
            for (unsigned at : {3U, 10U, 40U, 56U, 59U}) {
                const auto name =
                    "read-order-" + std::to_string(native) + "-" + std::to_string(first) + "-" + std::to_string(at);
                const unsigned state = native * 16;
                const unsigned expected = at > first ? 0 : at <= F::consumer_index ? state : 4;
                run(name.c_str(), [=](F &f) { f.read_span(first, 60, native, at); }, expected);
            }
    run("changed-owner", [](F &f) { f.m.put(F::owner + 0x94, 26U); }, 0);
    run("changed-writer", [](F &f) { f.m.put(F::writer_object + 0x30, 1279U); }, 0);
    run("bad-viewport", [](F &f) { f.m.put(F::viewport + 0x54, 2560U); }, 0);
    run("bad-backing-size", [](F &f) { f.m.put(F::backing + 0xb0, 1280U); }, 0);
    run("unstable-epoch", [](F &f) { f.m.mutate = F::renderer + 0x1f208; }, 0);
    run("wrong-epoch", [](F &f) { f.source.epoch = 8; }, 0);
    run("oversized-schedule", [](F &f) { f.m.put(F::renderer + 0x7af28, 32769U); }, 0);
    run("duplicate-writer", [](F &f) { f.m.put(F::schedule + 16 * 4 + 8, F::writer_object); }, 0);
    run("missing-consumer", [](F &f) { f.m.put(F::schedule + 16 * F::consumer_index + 8, F::writer_object); }, 0);
    run("nested-in-schedule", [](F &f) { f.m.put(F::schedule + 16 * 4 + 8, F::nested); }, 0);
    run(
        "late-writer",
        [](F &f) {
            f.m.put(F::schedule + 16 * F::writer_index + 8, F::consumer_object);
            f.m.put(F::schedule + 16 * F::consumer_index + 8, F::writer_object);
        },
        0);
    run("broken-chain", [](F &f) { f.m.put(F::spans + 0x40, std::uintptr_t{}); }, 0);
    run("tracker-immediate", [](F &f) { f.m.put(F::tracker + 0x30, std::uint64_t{10}); }, 0);
    run("wrong-use-ordinal", [](F &f) { f.m.put(F::spans + 8, 3U); }, 0);
    run("missing-writer-transition", [](F &f) { f.m.put(F::nested + 8, std::uintptr_t{}); }, 0);
    run("wrong-transition-span", [](F &f) { f.m.put(F::nodes + 0x10, F::spans + 0x28); }, 0);
    run("cyclic-state-nodes", [](F &f) { f.m.put(F::nodes, F::nodes); }, 0);
    run(
        "duplicate-hoist",
        [](F &f) {
            f.hoist();
            f.m.put(F::packets + 4 * 0x100 + 8, F::nodes + 0x20);
        },
        0);
    run(
        "unexpected-transition",
        [](F &f) {
            f.hoist();
            f.m.put(F::nodes + 0x3a, std::uint16_t{4});
        },
        0);
    run("foreign-backing-alias", [](F &f) { f.m.put(F::nodes + 8, F::backing + 8); }, 0);
    run(
        "unqualified-node-opcode",
        [](F &f) {
            f.hoist(4);
            f.m.put(F::packets + 4 * 0x100, 13U);
        },
        0);
    run("unreadable-node", [](F &f) { f.m.put(F::nested + 8, std::uintptr_t{1}); }, 0);
    run("direct-copy-opaque-child", [](F &f) { f.direct_copy(); }, 4);
    run("region-copy-opaque-child", [](F &f) { f.direct_copy(3); }, 4);
    run(
        "skipped-copy-opaque-child",
        [](F &f) {
            f.direct_copy(3);
            f.m.put(F::owner + 0x58, 6U);
            check(tb::read_owner(f.m.reader(), F::owner, f.depth), "updated owner kind");
        },
        4);
    run(
        "direct-copy-parent-transition",
        [](F &f) {
            f.direct_copy();
            f.hoist(4);
        },
        128);
    run(
        "direct-copy-parent-still-checked",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_copy + 8, std::uintptr_t{1440});
        },
        0);
    run(
        "copy-child-required-for-subrect",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_viewport + 0x54, 1280U);
        },
        0);
    run(
        "copy-child-required-for-offset",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_viewport + 0x4c, 1U);
        },
        0);
    run(
        "copy-child-required-for-height",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_viewport + 0x58, 720U);
        },
        0);
    run(
        "region-child-required-for-format",
        [](F &f) {
            f.direct_copy(3);
            f.m.put(F::source_backing + 0xc4, 42U);
        },
        0);
    run(
        "copy-primary-backing-precedence",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::source_owner + 0xf0, F::source_backing);
            f.m.put(F::source_owner + 0x60, std::uintptr_t{1});
        },
        4);
    run(
        "copy-branch-unreadable-owner",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_copy + 0x20, std::uintptr_t{1});
        },
        0);
    run(
        "copy-branch-unreadable-viewport",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_child + 0x10, std::uintptr_t{1});
        },
        0);
    run(
        "copy-branch-changes-between-snapshots",
        [](F &f) {
            f.direct_copy();
            f.m.mutate = F::source_owner + 0xf0;
        },
        0);
    run(
        "copy-child-draw-transition",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_viewport + 0x54, 1280U);
            f.m.put(F::other_child, 0U);
            f.m.put(F::other_child + 8, F::nodes + 0x20);
            f.m.put(F::packets + F::next_index * 0x100 + 8, std::uintptr_t{});
        },
        128);
    run(
        "copy-child-draw-invalid-node",
        [](F &f) {
            f.direct_copy();
            f.m.put(F::other_viewport + 0x54, 1280U);
            f.m.put(F::other_child, 0U);
        },
        0);
    // Whole copy (render scale 1.0): the depth is in the next span's read state
    // at the consumer only if that transition executed between writer and consumer.
    const auto whole = [](const char *name, auto configure, unsigned expected, bool mode = true) {
        F f;
        f.whole_copy();
        configure(f);
        auto proof = std::make_unique<cd::DepthConsumerAdmission>();
        const auto state = f.attempt(*proof, {}, true, true, mode);
        check(state == expected, name);
        std::cout << std::boolalpha << "{\"case\":\"" << name << "\",\"state\":" << state << ",\"trace\":";
        proof->write(std::cout);
        std::cout << "}\n";
    };
    whole("whole-copy-hoisted-read", [](F &) {}, 128);
    whole("whole-copy-combined-read", [](F &f) { f.whole_copy(12); }, 192);
    whole("whole-copy-compute-read", [](F &f) { f.whole_copy(4); }, 64);
    whole("whole-copy-read-at-consumer", [](F &f) { f.whole_copy(8, F::consumer_index); }, 128);
    whole(
        "whole-copy-writer-transition-on-writer",
        [](F &f) {
            f.m.put(F::packets + 1 * 0x100 + 8, std::uintptr_t{});
            f.m.put(F::writer_object + 8, F::nodes);
        },
        128);
    // game-76: the read span starts after the TAA dispatch; the depth is still COPY_DEST there.
    whole("whole-copy-read-after-consumer", [](F &f) { f.whole_copy(8, F::next_index); }, 1024);
    whole(
        "whole-copy-read-span-started-untransitioned",
        [](F &f) {
            f.whole_copy(8, F::next_index);
            f.m.put(F::spans + 0x28, F::packets + 40 * 0x100);
        },
        0);
    whole("whole-copy-read-at-writer", [](F &f) { f.whole_copy(8, F::writer_index); }, 0);
    whole("whole-copy-scaled-mode", [](F &) {}, 0, false);
    whole("whole-copy-subrectangle", [](F &f) { f.source.rectangle = {0, 0, 2560, 1438}; }, 0);
    whole("whole-copy-region-path", [](F &f) { f.m.put(F::writer_object + 0x10, 3U); }, 0);
    whole(
        "whole-copy-writer-not-copy-destination",
        [](F &f) {
            f.m.put(F::spans + 0x22, std::uint16_t{0x100});
            f.m.put(F::spans + 0x48, std::uint16_t{0x100});
            f.m.put(F::nodes + 0x1a, std::uint16_t{0x100});
            f.m.put(F::nodes + 0x38, std::uint16_t{0x100});
        },
        0);
    whole(
        "whole-copy-next-not-read",
        [](F &f) {
            f.m.put(F::spans + 0x4a, std::uint16_t{0x100});
            f.m.put(F::nodes + 0x3a, std::uint16_t{0x100});
        },
        0);
    whole(
        "whole-copy-extra-transition",
        [](F &f) {
            f.node(F::nodes + 0x40, F::spans + 0x28, 2048, 8);
            f.m.put(F::packets + 40 * 0x100 + 8, F::nodes + 0x40);
        },
        0);
    whole(
        "whole-copy-foreign-writer-node",
        [](F &f) {
            f.m.put(F::packets + 1 * 0x100 + 8, std::uintptr_t{});
            f.m.put(F::writer_object + 8, F::nodes);
            f.m.put(F::nodes + 0x18, std::uint16_t{8});
        },
        0);
    whole("whole-copy-span-on-child", [](F &f) { f.m.put(F::spans, F::nested); }, 0);
    {
        F f;
        auto proof = std::make_unique<cd::DepthConsumerAdmission>();
        check(f.attempt(*proof, {}, true, true, true) == 0, "scaled writer rejected in whole-copy mode");
    }
    {
        F f;
        f.m.put(F::nested + 8, std::uintptr_t{1});
        auto proof = std::make_unique<cd::DepthConsumerAdmission>();
        for (unsigned i = 0; i < 12; ++i) check(f.attempt(*proof) == 0, "bounded rejected capture");
        std::cout << std::boolalpha << "{\"case\":\"bounded-failure-telemetry\",\"state\":0,\"trace\":";
        proof->write(std::cout);
        std::cout << "}\n";
    }
    for (unsigned kind = 0; kind < 7; ++kind) {
        F f;
        auto proof = std::make_unique<cd::DepthConsumerAdmission>();
        D3D12_RESOURCE_BARRIER b{};
        b.Transition = {reinterpret_cast<ID3D12Resource *>(f.depth.resource), UINT_MAX,
                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET};
        bool available = true, description = true;
        if (kind == 1) b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (kind == 2) b.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
        if (kind == 3) b.Transition.Subresource = 0;
        if (kind == 4) {
            b = {};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        }
        if (kind == 5) available = false;
        if (kind == 6) description = false;
        check(f.attempt(*proof, std::span(&b, 1), available, description) == (kind ? 0U : 4U),
              "API conflict/lease/description gates");
    }
    {
        F f;
        auto proof = std::make_unique<cd::DepthConsumerAdmission>();
        for (unsigned i = 0; i < 12; ++i) {
            check(f.attempt(*proof) == 4, "repeated fresh capture");
            proof->copied();
        }
        std::cout << std::boolalpha << "{\"case\":\"bounded-telemetry\",\"state\":4,\"trace\":";
        proof->write(std::cout);
        std::cout << "}\n";
    }
}

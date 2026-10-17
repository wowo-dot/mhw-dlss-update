#pragma once
#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include "output_commands.hpp"
namespace mhw::scaled_temporal {
// Bounded identities only, under the route mutex. Neither references nor GPU
// work are owned here. A submission receipt is CPU observation, not completion.
class OutputDiagnostics {
public:
    using Words = std::array<std::uint64_t, 8>;
    struct Graph {
        // tick, frame, epoch, context, view, layer, bank, async
        Words at{};
        // filter descriptor, stage, order, unit, type, input owner, output owner
        std::array<std::array<std::uint64_t, 7>, 16> filters{};
        // motion unit, grade unit, endpoint, motion input/output, grade input/output, final source
        Words links{};
        // Optional qualification: high 16 bits are the effect stage; low bits name
        // the failed geometry/input check. Zero means no finer result is available.
        unsigned count{}, reason{}, detail{}, link_reads{}, qualification{};
        // Only on qualification check 3: packet, IA, context, count, first,
        // successful-read mask, stride, byte offset, CPU arena, failed predicate.
        std::array<std::uint64_t, 10> vertex_gate{};
        OutputCommands::Gate command_gate{};
    };
    // tick, list, generation, recording, executor, context, packet, returned,
    // validated API draw, callback frame, callback epoch
    using Draw = std::array<std::uint64_t, 11>;
    // list, generation, queue, batch ordinal, index, count, tick
    using Submission = std::array<std::uint64_t, 7>;
    struct Order {
        std::uint64_t ordinal{}, frame{}, epoch{}, tick{};
        unsigned executed{};
        Words identity{}; // plan frame/epoch/context/view/bank/async, request context/view
        std::array<std::uint64_t, 4> packets{};
        std::array<Draw, 4> draws{};
        std::array<Submission, 5> submissions{}; // temporal, then each effect
    };

private:
    struct GraphSample {
        std::uint64_t ordinal{};
        Graph value;
    };
    GraphSample graph_baseline_{}, graph_latest_{};
    std::array<GraphSample, 4> graph_first_{};
    std::uint64_t graph_attempts_{}, graph_accepted_{}, graph_rejected_{}, order_count_{}, batches_{},
        submission_count_{};
    std::array<Order, 4> orders_{};
    Order latest_{};
    std::array<Submission, 128> history_{};
    static bool same(const Submission &a, const Submission &b) { return a[0] && a[1] && a[0] == b[0] && a[1] == b[1]; }
    void lookup(Submission &s) const {
        for (const auto &h : history_)
            if (same(s, h) && (!s[3] || h[3] < s[3])) s = h;
    }
    template <class F> void each(F f) {
        for (unsigned i = 0; i < orders_.size() && i < order_count_; ++i) f(orders_[i]);
        if (order_count_ > orders_.size()) f(latest_);
    }
    static void array(std::ostream &o, const auto &v) {
        o << '[';
        bool comma = false;
        for (auto x : v) {
            if (comma) o << ',';
            o << x;
            comma = true;
        }
        o << ']';
    }
    static void write_graph(std::ostream &o, const GraphSample &s) {
        const auto &v = s.value;
        const auto a = [&](const auto &x) { array(o, x); };
        o << "{\"ordinal\":" << s.ordinal << ",\"at\":";
        a(v.at);
        o << ",\"count\":" << v.count << ",\"reason\":" << v.reason << ",\"detail\":" << v.detail
          << ",\"qualification\":" << v.qualification << ",\"link_reads\":" << v.link_reads << ",\"links\":";
        a(v.links);
        o << ",\"vertex_gate\":";
        if ((v.qualification & 65535U) == 3U)
            a(v.vertex_gate);
        else
            o << "null";
        o << ",\"command_gate\":";
        if (v.command_gate[0])
            a(v.command_gate);
        else
            o << "null";
        o << ",\"filters\":[";
        for (unsigned i = 0; i < v.count; ++i) {
            if (i) o << ',';
            a(v.filters[i]);
        }
        o << "]}";
    }

public:
    OutputCommands commands;
    std::uint64_t rejected_graphs() const { return graph_rejected_; }
    // The latest rejected plan graph (filters, links, failed gate), or null.
    void write_latest_rejection(std::ostream &o) const {
        if (graph_rejected_)
            write_graph(o, graph_latest_);
        else
            o << "null";
    }
    void graph(const Graph &g, bool accepted) {
        ++graph_attempts_;
        if (accepted) {
            if (!graph_accepted_++) graph_baseline_ = {graph_attempts_, g};
            return;
        }
        ++graph_rejected_;
        graph_latest_ = {graph_rejected_, g};
        if (graph_rejected_ <= graph_first_.size())
            graph_first_[static_cast<unsigned>(graph_rejected_ - 1)] = graph_latest_;
    }
    void order(std::uint64_t frame, std::uint64_t epoch, std::uint64_t tick, unsigned executed, std::uint64_t list,
               std::uint64_t generation, const std::array<std::uint64_t, 4> &packets, const std::array<Draw, 4> &draws,
               Words identity = {}) {
        Order o;
        o.ordinal = ++order_count_;
        o.frame = frame;
        o.epoch = epoch;
        o.tick = tick;
        o.executed = executed;
        o.packets = packets;
        o.draws = draws;
        o.identity = identity;
        o.submissions[0][0] = list;
        o.submissions[0][1] = generation;
        lookup(o.submissions[0]);
        for (unsigned i = 0; i < 4; ++i) {
            o.submissions[i + 1][0] = draws[i][1];
            o.submissions[i + 1][1] = draws[i][2];
            lookup(o.submissions[i + 1]);
        }
        if (order_count_ <= orders_.size())
            orders_[static_cast<unsigned>(order_count_ - 1)] = o;
        else
            latest_ = o;
    }
    void draw(std::uint64_t frame, std::uint64_t epoch, unsigned stage, const Draw &d) {
        if (stage >= 4) return;
        each([&](Order &o) {
            if (o.frame != frame || o.epoch != epoch || o.packets[stage] != d[6]) return;
            o.draws[stage] = d;
            auto &s = o.submissions[stage + 1];
            if (s[0] != d[1] || s[1] != d[2]) s = {};
            s[0] = d[1];
            s[1] = d[2];
            lookup(s);
        });
    }
    void submit(std::uint64_t queue, std::uint64_t tick, std::span<const std::array<std::uint64_t, 2>> lists) {
        const auto batch = ++batches_;
        for (unsigned i = 0; i < lists.size(); ++i) {
            if (!lists[i][0] || !lists[i][1]) continue;
            Submission h{lists[i][0], lists[i][1], queue, batch, i, lists.size(), tick};
            history_[submission_count_++ % history_.size()] = h;
            each([&](Order &o) {
                for (auto &s : o.submissions)
                    if (same(s, h) && !s[3]) s = h;
            });
        }
    }
    void write(std::ostream &o) const {
        const auto a = [&](const auto &v) { array(o, v); };
        const auto graph = [&](const GraphSample &s) { write_graph(o, s); };
        const auto order = [&](const Order &v) {
            o << "{\"ordinal\":" << v.ordinal << ",\"frame\":" << v.frame << ",\"epoch\":" << v.epoch
              << ",\"tick\":" << v.tick << ",\"executed\":" << v.executed << ",\"packets\":";
            a(v.packets);
            o << ",\"identity\":";
            a(v.identity);
            o << ",\"draws\":[";
            for (unsigned i = 0; i < 4; ++i) {
                if (i) o << ',';
                a(v.draws[i]);
            }
            o << "],\"submissions\":[";
            for (unsigned i = 0; i < 5; ++i) {
                if (i) o << ',';
                a(v.submissions[i]);
            }
            o << "]}";
        };
        o << "{\"schema\":2,\"policy\":\"bounded-output-correlations-v2\",\"diagnostic_only\":true,\"capacity\":4,\"history_capacity\":128,\"graph_attempts\":"
          << graph_attempts_ << ",\"graph_accepted\":" << graph_accepted_ << ",\"graph_rejected\":" << graph_rejected_
          << ",\"order_count\":" << order_count_ << ",\"batches\":" << batches_
          << ",\"submission_count\":" << submission_count_;
        o << ",\"vertex_policy\":\"native-overlay-layout-v1\"";
        o << ",\"command_policy\":\"prepared-command-rejections-v1\"";
        o << ",\"commands\":";
        commands.write(o);
        o << ",\"graph_baseline\":";
        if (graph_accepted_)
            graph(graph_baseline_);
        else
            o << "null";
        o << ",\"graphs\":[";
        for (unsigned i = 0; i < graph_first_.size() && i < graph_rejected_; ++i) {
            if (i) o << ',';
            graph(graph_first_[i]);
        }
        if (graph_rejected_ > graph_first_.size()) {
            o << ',';
            graph(graph_latest_);
        }
        o << "],\"orders\":[";
        for (unsigned i = 0; i < orders_.size() && i < order_count_; ++i) {
            if (i) o << ',';
            order(orders_[i]);
        }
        if (order_count_ > orders_.size()) {
            o << ',';
            order(latest_);
        }
        o << "]}";
    }
};
} // namespace mhw::scaled_temporal

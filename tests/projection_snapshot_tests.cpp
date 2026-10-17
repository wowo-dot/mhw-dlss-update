#include "projection_snapshot.hpp"
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace b = mhw::bridge_probe;
using A = std::uintptr_t;
void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
struct Fixture {
    static constexpr A base = 0x10000000, renderer = 0x20000000, context = renderer + 0x710, view = 0x30000000,
                       table = 0x40000000, manager = 0x50000000, metadata_table = 0x60000000, metadata = 0x70000000,
                       constants = 0x80000000;
    b::ProjectionAnchor anchor{renderer, context, view, table, 31, 0x100000021ULL, 1, 1};
    std::map<A, std::byte> memory;
    std::vector<A> reads;
    unsigned fail{}, mutate{};
    template <class T> void put(A at, T value) {
        const auto bytes = std::as_bytes(std::span(&value, 1));
        for (std::size_t i = 0; i < bytes.size(); ++i) memory[at + i] = bytes[i];
    }
    Fixture() {
        put(base + 0x51c4480, renderer);
        put(base + 0x51c4368, table);
        put(base + 0x51c4400, manager);
        put(renderer + 0x1f450, view);
        put(renderer + 0x1f210, anchor.frame);
        put(renderer + 0x1f208, anchor.epoch);
        put(renderer + 0x1f200, 1U);
        put(renderer + 0x93, static_cast<unsigned char>(1));
        put(manager, base + 0x350d188);
        put(manager + 0x40, static_cast<unsigned>(anchor.epoch));
        put(base + 0x4be75f4, 1U);
        put(base + 0x4be7604, 0xf00U);
        put(base + 0x4be763c, 0x10f00U);
        put(context + 0x2b18, metadata_table);
        put(metadata_table + 8, metadata);
        put(metadata + 0x10, 32U);
        put(context + 0x718, constants | A{3});
        put(context + 0x230, 2560.f);
        put(context + 0x234, 1440.f);
        put(manager + 0x38, .46875f);
        put(manager + 0x3c, -.04320985f);
        put(table + 0xe6a0 + 8 * (anchor.frame % 20), .46875f);
        put(table + 0xe6a0 + 8 * (anchor.frame % 20) + 4, -.04320985f);
        for (unsigned i = 0; i < 16; ++i) {
            const float v = i / 4 == i % 4 ? 1.f : 0.f;
            put(constants + 4 * i, v);
            put(constants + 64 + 4 * i, v);
        }
        put(constants + 48, .46875f / 2560.f);
        put(constants + 52, -.04320985f / 1440.f);
    }
    static b::ScalarReadResult read(void *opaque, A at, std::span<std::byte> out) noexcept {
        auto &f = *static_cast<Fixture *>(opaque);
        f.reads.push_back(at);
        if (f.fail == f.reads.size()) return b::ScalarReadResult::fault;
        for (std::size_t i = 0; i < out.size(); ++i) {
            const auto it = f.memory.find(at + i);
            if (it == f.memory.end()) return b::ScalarReadResult::fault;
            out[i] = it->second;
        }
        if (f.mutate == f.reads.size()) out[0] ^= std::byte{1};
        return b::ScalarReadResult::success;
    }
    b::ProjectionSnapshot capture() {
        reads.clear();
        return b::capture_projection_snapshot(base, anchor, read, this);
    }
};
int main() {
    try {
        Fixture f;
        const auto good = f.capture();
        const auto sequence = f.reads;
        require(good.valid && good.matrix_relation && good.checkpoint == 100 && good.reads <= 192,
                "complete CPU projection capture");
        require(good.epoch == 33 && good.constants == Fixture::constants &&
                    good.tagged_constants == (Fixture::constants | 3),
                "epoch narrowing and pointer tags");
        require(good.rejection == b::ProjectionReject::none && good.vtable_rva == 0x350d188,
                "base camera type retained");
        {
            Fixture x;
            x.put(Fixture::manager, Fixture::base + A{0x33f82a8});
            const auto s = x.capture();
            require(s.valid && s.matrix_relation && s.vtable_rva == 0x33f82a8 && s.vtable == Fixture::base + 0x33f82a8,
                    "derived camera retains base projection layout");
        }
        {
            Fixture x;
            x.put(Fixture::manager, Fixture::base + A{0x33f82b0});
            const auto s = x.capture();
            require(!s.valid && s.rejection == b::ProjectionReject::camera_type && s.reads == 14 &&
                        s.vtable_rva == 0x33f82b0,
                    "unknown derived type rejected and retained");
        }
        {
            Fixture x;
            x.put(Fixture::manager + 0x40, 34U);
            require(x.capture().rejection == b::ProjectionReject::camera_epoch,
                    "epoch rejection distinguished from type");
        }
        {
            Fixture x;
            x.put(Fixture::base + 0x4be75f4, 0xffffffffU);
            require(x.capture().rejection == b::ProjectionReject::buffer_unavailable, "missing buffer rejection");
        }
        {
            Fixture x;
            x.put(Fixture::renderer + 0x93, static_cast<unsigned char>(0));
            require(x.capture().rejection == b::ProjectionReject::source_changed,
                    "initial anchor mutation distinguished");
        }
        for (unsigned n = 1; n <= good.reads; ++n) {
            Fixture x;
            x.fail = n;
            const auto s = x.capture();
            require(!s.valid && s.rejection == b::ProjectionReject::read,
                    "every failed read closes capture with read reason");
        }
        std::map<A, unsigned> seen;
        unsigned mutations{};
        for (unsigned i = 0; i < sequence.size(); ++i)
            if (++seen[sequence[i]] == 2) {
                Fixture x;
                x.mutate = i + 1;
                require(!x.capture().valid, "mutable fields must survive reread");
                ++mutations;
            }
        for (const auto id : {0xffffffffU, 0U, 0xe00U, 0x3fff00U, 0x1000U}) {
            Fixture x;
            x.put(Fixture::base + 0x4be7604, id);
            require(!x.capture().valid, "matrix shape and bounds");
        }
        {
            Fixture x;
            x.put(Fixture::base + 0x4be763c, 0x8f00U);
            require(!x.capture().valid, "overlapping matrices");
        }
        {
            Fixture x;
            x.put(Fixture::metadata + 0x10, 4097U);
            require(!x.capture().valid, "bounded metadata size");
        }
        {
            Fixture x;
            x.put(Fixture::context + 0x718, A{15});
            require(!x.capture().valid, "null tagged constants");
        }
        {
            Fixture x;
            x.put(Fixture::manager + 0x40, 34U);
            require(!x.capture().valid, "camera stamp must match source epoch");
        }
        {
            Fixture x;
            x.put(Fixture::manager, A{0});
            require(!x.capture().valid, "exact camera type");
        }
        {
            Fixture x;
            x.anchor.context++;
            require(!x.capture().valid, "recording context identity");
        }
        {
            Fixture x;
            x.anchor.renderer = (std::numeric_limits<A>::max)();
            require(!x.capture().valid, "anchor overflow");
        }
        for (const auto at :
             {Fixture::constants, Fixture::constants + 64, Fixture::manager + 0x38, Fixture::context + 0x230}) {
            Fixture x;
            x.put(at, std::numeric_limits<float>::quiet_NaN());
            require(!x.capture().valid, "non-finite snapshot");
        }
        for (const auto extent : {0.f, -1.f}) {
            Fixture x;
            x.put(Fixture::context + 0x234, extent);
            require(!x.capture().valid, "invalid viewport");
        }
        {
            Fixture x;
            x.put(Fixture::constants + 48, 0.f);
            const auto s = x.capture();
            require(s.valid && !s.matrix_relation, "stable mismatch retained for diagnosis");
        }
        {
            auto s = good;
            s.valid = false;
            require(!b::projection_relation_matches(s), "invalid snapshot cannot match");
        }
        {
            auto s = good;
            for (unsigned i = 0; i < 16; ++i) s.projection_bits[i] = s.unjittered_bits[i] = 0;
            require(!b::projection_relation_matches(s), "degenerate homogeneous column");
        }
        {
            auto s = good;
            s.unjittered_bits[11] = std::bit_cast<unsigned>(1.f);
            s.projection_bits[11] = s.unjittered_bits[11];
            s.projection_bits[8] = s.projection_bits[12];
            s.projection_bits[9] = s.projection_bits[13];
            s.unjittered_bits[15] = s.projection_bits[15] = 0;
            s.projection_bits[12] = s.projection_bits[13] = 0;
            require(b::projection_relation_matches(s), "perspective row-vector relation");
            s.projection_bits[9] = std::bit_cast<unsigned>(-std::bit_cast<float>(s.projection_bits[9]));
            require(!b::projection_relation_matches(s), "wrong jitter sign");
        }
        std::cout << "Projection capture passed: " << good.reads << " failed-read cases, " << mutations
                  << " unstable-read cases, layout and transform checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

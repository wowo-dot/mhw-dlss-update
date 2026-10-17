#pragma once
#include <array>
#include <cstdint>
#include <ostream>

namespace mhw::continuous_dlaa {
// Bounded typed metadata only. The runtime updates this under its existing lock;
// it does not dereference these recorded addresses or change request ownership.
struct RequestIdentity {
    std::uint64_t frame{}, epoch{}, sequence{}, microseconds{}, packet{}, state{}, view{}, context{}, layer{}, depth{};
    unsigned bank{}, async{};
    std::uint64_t bypasses{}, last_bypass_tick{}, last_bypass_caller{};
    unsigned last_bypass_publishers{};
    void write(std::ostream &o) const {
        o << "{\"frame\":" << frame << ",\"epoch\":" << epoch << ",\"sequence\":" << sequence
          << ",\"microseconds\":" << microseconds << ",\"packet\":" << packet << ",\"state\":" << state
          << ",\"view\":" << view << ",\"context\":" << context << ",\"layer\":" << layer << ",\"depth\":" << depth
          << ",\"bank\":" << bank << ",\"async\":" << async << ",\"bypasses\":" << bypasses
          << ",\"last_bypass_tick\":" << last_bypass_tick << ",\"last_bypass_caller\":" << last_bypass_caller
          << ",\"last_bypass_publishers\":" << last_bypass_publishers << '}';
    }
};
struct ConsumerSample {
    std::uint64_t ordinal{}, tick{}, packet{}, caller{};
    unsigned publishers{}, live{}, matches{};
    RequestIdentity newest;
    void write(std::ostream &o) const {
        o << "{\"ordinal\":" << ordinal << ",\"tick\":" << tick << ",\"packet\":" << packet << ",\"caller\":" << caller
          << ",\"publishers\":" << publishers << ",\"live\":" << live << ",\"matches\":" << matches << ",\"newest\":";
        newest.write(o);
        o << '}';
    }
};
struct ConsumerCounts {
    std::uint64_t calls{}, blocked{}, unmatched{}, matched{}, changed{}, reentrant{}, multiple{};
    ConsumerSample first_blocked, last_blocked, first_unmatched, last_unmatched, first_multiple, last_multiple;
    void write(std::ostream &o) const {
        o << "{\"calls\":" << calls << ",\"blocked\":" << blocked << ",\"unmatched\":" << unmatched
          << ",\"matched\":" << matched << ",\"changed\":" << changed << ",\"reentrant\":" << reentrant
          << ",\"multiple\":" << multiple;
        const auto pair = [&](const char *name, std::uint64_t count, const ConsumerSample &first,
                              const ConsumerSample &last) {
            o << ",\"first_" << name << "\":";
            if (count)
                first.write(o);
            else
                o << "null";
            o << ",\"last_" << name << "\":";
            if (count)
                last.write(o);
            else
                o << "null";
        };
        pair("blocked", blocked, first_blocked, last_blocked);
        pair("unmatched", unmatched, first_unmatched, last_unmatched);
        pair("multiple", multiple, first_multiple, last_multiple);
        o << '}';
    }
};
struct RequestCollision {
    std::uint64_t tick{}, publication_attempt{}, lease_list{}, lease_generation{}, lease_frame{};
    unsigned slot{}, live{}, publishers{}, pending_callbacks{};
    bool lease_depth{}, lease_evaluated{};
    RequestIdentity old_request, new_request;
    void write(std::ostream &o) const {
        o << "{\"tick\":" << tick << ",\"publication_attempt\":" << publication_attempt << ",\"slot\":" << slot
          << ",\"live\":" << live << ",\"publishers\":" << publishers << ",\"pending_callbacks\":" << pending_callbacks
          << ",\"lease_list\":" << lease_list << ",\"lease_generation\":" << lease_generation
          << ",\"lease_frame\":" << lease_frame << ",\"lease_depth\":" << lease_depth
          << ",\"lease_evaluated\":" << lease_evaluated << ",\"old_request\":";
        old_request.write(o);
        o << ",\"new_request\":";
        new_request.write(o);
        o << '}';
    }
};
struct RequestTelemetry {
    std::uint64_t publication_attempts{}, published{}, collisions{};
    std::array<ConsumerCounts, 2> consumers; // TAA, depth.
    RequestCollision first_collision;
    void write(std::ostream &o) const {
        o << std::boolalpha
          << "{\"schema\":1,\"policy\":\"observe-existing-request-lifetime\",\"capacity\":32,\"publication_attempts\":"
          << publication_attempts << ",\"published\":" << published << ",\"collisions\":" << collisions << ",\"taa\":";
        consumers[0].write(o);
        o << ",\"depth\":";
        consumers[1].write(o);
        o << ",\"first_collision\":";
        if (collisions)
            first_collision.write(o);
        else
            o << "null";
        o << '}';
    }
};
static_assert(sizeof(RequestTelemetry) < 4096);

// Separate from the observation-only request counters, for versioned consumers.
// pending includes only a uniquely identified published request whose native
// consumer is currently forwarding. Retire only after its successful return.
struct ForwardedRequestTelemetry {
    std::uint64_t candidates{}, unmatched{}, ambiguous{}, invalid{}, started{}, pending{}, retired{}, failed{};
    RequestIdentity first_retired, last_retired;
    void write(std::ostream &o) const {
        o << "{\"schema\":1,\"policy\":\"validated-native-return-v1\",\"candidates\":" << candidates
          << ",\"unmatched\":" << unmatched << ",\"ambiguous\":" << ambiguous << ",\"invalid\":" << invalid
          << ",\"started\":" << started << ",\"pending\":" << pending << ",\"retired\":" << retired
          << ",\"failed\":" << failed << ",\"first_retired\":";
        if (retired)
            first_retired.write(o);
        else
            o << "null";
        o << ",\"last_retired\":";
        if (retired)
            last_retired.write(o);
        else
            o << "null";
        o << '}';
    }
};
} // namespace mhw::continuous_dlaa

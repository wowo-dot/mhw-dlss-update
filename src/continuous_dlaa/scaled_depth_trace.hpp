#pragma once
#include <array>
#include <cstdint>
#include <ostream>
namespace mhw::continuous_dlaa {
// Diagnostic values only. The caller serializes access; this class never reads
// pointers, changes admission, owns GPU resources, or allocates per frame.
struct ScaledDepthTrace {
    static constexpr unsigned stages=7,reasons=16,capacity=128,event_limit=32,sample_limit=5;
    using Values=std::array<std::uint64_t,16>;
    struct Event {
        std::uint64_t tick{},frame{},epoch{},depth_sequence{};
        unsigned stage{},reason{};Values values{};
    };
    struct Missing {
        std::uint64_t ordinal{};
        // tick, frame, epoch, temporal sequence, selected depth sequence,
        // temporal packet, command list, command generation
        std::array<std::uint64_t,8> expected{};
        std::array<std::uint64_t,5> actual{};
        std::uint64_t matching{},ring_overwritten{};
        unsigned saved{};std::array<Event,event_limit> events{};
    };
    std::array<std::array<std::uint64_t,reasons>,stages> counts{};
    std::array<Event,capacity> ring{};
    std::array<Missing,sample_limit> samples{};
    std::uint64_t total{},uncorrelated{},missing_count{},readiness_rejected{};
    void observe(const Event &e) noexcept {
        if(e.stage>=stages||e.reason>=reasons)return;
        ++counts[e.stage][e.reason];
        if(!e.frame||!e.epoch){++uncorrelated;return;}
        ring[total++%capacity]=e;
    }
    void missing(const std::array<std::uint64_t,8> &expected,const std::array<std::uint64_t,5> &actual) noexcept {
        const auto ordinal=++missing_count;
        auto &s=samples[ordinal<sample_limit?static_cast<unsigned>(ordinal-1):sample_limit-1];s={};
        s.ordinal=ordinal;s.expected=expected;s.actual=actual;s.ring_overwritten=total>capacity?total-capacity:0;
        const auto start=total>capacity?total-capacity:0;
        for(auto i=start;i<total;++i){const auto &e=ring[i%capacity];
            if(e.frame!=expected[1]||e.epoch!=expected[2])continue;
            ++s.matching;
            // First 31 and latest if this frame exceeds the bounded sample.
            if(s.saved<event_limit)s.events[s.saved++]=e;else s.events.back()=e;
        }
    }
    template<class T>static void array(std::ostream &o,const T &a){o<<'[';bool comma=false;for(const auto &v:a){o<<(comma?",":"")<<v;comma=true;}o<<']';}
    void write(std::ostream &o)const {
        o<<"{\"schema\":1,\"policy\":\"bounded-scaled-depth-admission-v1\",\"diagnostic_only\":true,\"capacity\":"<<capacity
         <<",\"event_limit\":"<<event_limit<<",\"sample_limit\":"<<sample_limit<<",\"events\":"<<total<<",\"uncorrelated\":"<<uncorrelated
         <<",\"missing\":"<<missing_count<<",\"readiness_rejected\":"<<readiness_rejected<<",\"counts\":[";
        for(unsigned i=0;i<stages;++i){o<<(i?",":"");array(o,counts[i]);}o<<"],\"samples\":[";bool comma=false;
        for(const auto &s:samples)if(s.ordinal){o<<(comma?",":"")<<"{\"ordinal\":"<<s.ordinal<<",\"expected\":";comma=true;array(o,s.expected);
            o<<",\"actual\":";array(o,s.actual);o<<",\"matching\":"<<s.matching<<",\"ring_overwritten\":"<<s.ring_overwritten<<",\"events\":[";
            for(unsigned i=0;i<s.saved;++i){const auto &e=s.events[i];o<<(i?",":"")<<'['<<e.tick<<','<<e.frame<<','<<e.epoch<<','<<e.depth_sequence<<','<<e.stage<<','<<e.reason;
                for(auto v:e.values)o<<','<<v;o<<']';}o<<"]}";
        }o<<"]}";
    }
};
}

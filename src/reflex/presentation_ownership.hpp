#pragma once
#include <array>
#include <cstdint>

namespace mhw::reflex {
// CPU-only bounded diagnostic. Caller serializes access. Pointer values are
// identities only: this recorder neither owns nor dereferences host objects.
struct OwnershipCall {
    unsigned method{}, phase{}, thread{};
    std::uint32_t result{};
    std::uint64_t sequence{}, qpc{}, caller{}, object{}, returned{};
    std::array<std::uint64_t,8> args{};
};
struct OwnershipBucket {
    unsigned method{}, phase{}, thread{};
    std::uint64_t caller{}, count{};
};
class PresentationOwnership final {
public:
    static constexpr unsigned bucket_limit=64, sample_limit=512;
    std::array<OwnershipBucket,bucket_limit> buckets{};
    std::array<OwnershipCall,sample_limit> samples{};
    unsigned bucket_count{}, sample_count{}, phase{};
    std::uint64_t calls{}, dropped_buckets{}, dropped_samples{};
    void record(OwnershipCall call) noexcept {
        call.phase=phase; call.sequence=++calls;
        unsigned i{};
        for(;i<bucket_count;++i) {
            const auto& b=buckets[i];
            if(b.method==call.method && b.phase==call.phase &&
               b.thread==call.thread && b.caller==call.caller)break;
        }
        if(i==bucket_count) {
            if(bucket_count==bucket_limit){++dropped_buckets;return;}
            buckets[bucket_count++]={call.method,call.phase,call.thread,call.caller,0};
        }
        const auto n=++buckets[i].count;
        // First four calls, then exponential checkpoints. Counts remain exact
        // even after sampling ends. Changing buffer identities are not proven
        // absent between these samples; this is an ownership map, not a trace.
        if(n>4 && (n>65536 || (n&(n-1))))return;
        if(sample_count==sample_limit){++dropped_samples;return;}
        samples[sample_count++]=call;
    }
};
}

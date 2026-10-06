#pragma once
#include <array>
#include <cstdint>
#include <ostream>
namespace mhw::scaled_temporal {
// All values are copies of already-inspected metadata. Diagnostic only: a
// receipt never grants resource-state admission or substitutes for a barrier.
struct DrawStateReceipt {
    // tick, frame, epoch, stage, packet, list, generation
    std::array<std::uint64_t,7> at{};
    // tracking, state-known, state, expected-output, actual-output,
    // raster-readable, target-known, expected-RTV, actual-RTV,
    // expected-PSO, actual-PSO, expected-root, actual-root
    std::array<std::uint64_t,13> values{};
    unsigned mask{};
};
class DrawStateTrace {
    std::uint64_t count_{};DrawStateReceipt first_{},latest_{};
public:
    void reject(const DrawStateReceipt &s){if(!count_++)first_=s;latest_=s;}
    const DrawStateReceipt &latest()const noexcept{return latest_;}
    void write(std::ostream &o)const {
        o<<"{\"schema\":1,\"diagnostic_only\":true,\"count\":"<<count_<<",\"samples\":[";
        const auto emit=[&](const DrawStateReceipt &s){
            o<<"{\"mask\":"<<s.mask<<",\"at\":[";
            for(unsigned i=0;i<s.at.size();++i)o<<(i?",":"")<<s.at[i];o<<"],\"values\":[";
            for(unsigned i=0;i<s.values.size();++i)o<<(i?",":"")<<s.values[i];o<<"]}";
        };
        if(count_)emit(first_);if(count_>1){o<<',';emit(latest_);}o<<"]}";
    }
};
}

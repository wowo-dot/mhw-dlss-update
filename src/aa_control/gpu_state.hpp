#pragma once
#include <cstdint>
namespace mhw::aa_control::gpu_trace {
enum class Issue : unsigned {none,unknown,split,alias,subresource,conflict,order,unsupported};
struct State {
    bool known{};unsigned value{},transitions{};Issue issue{Issue::unknown};
    std::uint64_t last{};
    void invalidate(Issue why,std::uint64_t order) noexcept {known=false;issue=why;last=order;}
    void transition(unsigned before,unsigned after,unsigned flags,unsigned subresource,std::uint64_t order) noexcept {
        if(order<=last){invalidate(Issue::order,order);return;}
        // Inputs are verified single-plane/mip/slice. Subresource zero and ALL
        // cover the same image; split/alias history is deliberately not guessed.
        if(flags){invalidate(Issue::split,order);return;}
        if(subresource!=0&&subresource!=0xffffffffU){invalidate(Issue::subresource,order);return;}
        if(issue!=Issue::none&&issue!=Issue::unknown){last=order;return;}
        if(known&&value!=before){invalidate(Issue::conflict,order);return;}
        known=true;value=after;issue=Issue::none;last=order;++transitions;
    }
};
}

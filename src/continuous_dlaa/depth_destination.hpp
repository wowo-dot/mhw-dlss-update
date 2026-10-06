#pragma once
#include "depth_eligibility.hpp"
namespace mhw::continuous_dlaa {
// The source attachment is captured and validated at the native depth copy.
// Its later mutable context metadata is not the copied destination's identity.
// Full producer-time SceneDepth equality and all copy/lease gates remain intact.
inline bool depth_destination_equal(const temporal_bindings::SceneDepth &expected,
                                    const temporal_bindings::SceneDepth &actual,bool readable) noexcept {
    return readable&&expected.container==actual.container&&expected.texture==actual.texture&&
        expected.srv==actual.srv&&expected.cpu_descriptor==actual.cpu_descriptor&&expected.srv_readable==actual.srv_readable;
}
class DepthDestinationTrace {
    using Identity=DepthEligibilityTrace::Identity;
    struct Sample {std::uint64_t ordinal{};Identity at;unsigned difference{};
        std::array<std::uint64_t,21> expected{},actual{};};
    std::uint64_t checks_{},accepted_{},attachment_only_{};
    std::array<Sample,8> first_{};Sample latest_{};
    template<std::size_t N>static void array(std::ostream &out,const std::array<std::uint64_t,N> &v){out<<'[';for(unsigned i=0;i<N;++i)out<<(i?",":"")<<v[i];out<<']';}
public:
    // Same runtime mutex as the existing diagnostics; no game reads or writes.
    bool check(const Identity &at,const temporal_bindings::SceneDepth &expected,
               const temporal_bindings::SceneDepth &actual,bool readable) noexcept {
        ++checks_;if(!depth_destination_equal(expected,actual,readable))return false;++accepted_;
        if(expected==actual)return true;
        Sample sample;sample.ordinal=++attachment_only_;sample.at=at;
        sample.expected=depth_fields(expected);sample.actual=depth_fields(actual);sample.difference=depth_difference(sample.expected,sample.actual);
        if(attachment_only_<=first_.size())first_[static_cast<std::size_t>(attachment_only_-1)]=sample;else latest_=sample;
        return true;
    }
    void write(std::ostream &out)const {
        out<<"{\"schema\":1,\"policy\":\"depth-destination-identity-v1\",\"checks\":"<<checks_<<",\"accepted\":"<<accepted_
           <<",\"rejected\":"<<checks_-accepted_<<",\"attachment_only\":"<<attachment_only_<<",\"samples\":[";
        const auto emit=[&](const Sample &s){const auto &a=s.at;out<<"{\"ordinal\":"<<s.ordinal<<",\"at\":";
            array(out,std::array<std::uint64_t,11>{a.tick,a.frame,a.epoch,a.sequence,a.packet,a.list,a.generation,a.view,a.context,a.bank,a.async});
            out<<",\"difference\":"<<s.difference<<",\"expected\":";array(out,s.expected);out<<",\"actual\":";array(out,s.actual);out<<'}';};
        const auto count=static_cast<unsigned>((std::min)(attachment_only_,std::uint64_t{8}));
        for(unsigned i=0;i<count;++i){if(i)out<<',';emit(first_[i]);}if(attachment_only_>8){out<<',';emit(latest_);}out<<"]}";
    }
};
}

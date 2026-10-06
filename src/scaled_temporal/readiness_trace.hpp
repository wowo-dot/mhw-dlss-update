#pragma once
#include <array>
#include <cstdint>
#include <ostream>
namespace mhw::scaled_temporal {
// Observation only. Called under the existing stream-route mutex; never grants
// admission, reads native memory, retains resources or issues GPU commands.
class ReadinessTrace {
public:
    enum Reason : unsigned {accepted,inactive,faulted,active_route,arguments,unbuilt,claimed,executed,
        stamp,pipeline_missing,pipeline_signature,packet,retention,reason_count};
    struct Snapshot {
        // tick, request frame/epoch, command list/generation, native recording
        std::array<std::uint64_t,6> at{};
        // context, view, bank, async
        std::array<std::uint64_t,4> request{};
        // slot, frame, epoch, published, built, claimed, executed mask, packet,
        // context, view, bank, async
        std::array<std::uint64_t,12> plan{};
        // slot, list, generation, recording, resolved, copied, draws, frame, epoch
        std::array<std::uint64_t,9> active{};
        unsigned fault{},pipeline_missing_mask{},pipeline_signature_mask{},stage{4};
        bool accepting{},stamp_equal{};
    };
private:
    struct Sample {Snapshot value;std::uint64_t ordinal{},rejection{};Reason reason{};};
    Sample baseline_{},latest_{};std::array<Sample,8> first_{};
    std::uint64_t attempts_{},accepted_{},rejected_{};
    std::array<std::uint64_t,reason_count> reasons_{};
public:
    template<class Capture>void note(Reason reason,Capture capture) noexcept {
        ++attempts_;++reasons_[reason];
        if(reason==accepted){if(!accepted_++)baseline_={capture(),attempts_,0,reason};return;}
        ++rejected_;latest_={capture(),attempts_,rejected_,reason};
        if(rejected_<=first_.size())first_[static_cast<unsigned>(rejected_-1)]=latest_;
    }
    void write(std::ostream &o,unsigned schema=2)const {
        const auto array=[&](const auto &a){o<<'[';bool comma=false;for(const auto v:a){if(comma)o<<',';o<<v;comma=true;}o<<']';};
        const auto sample=[&](const Sample &s){const auto &v=s.value;
            o<<"{\"ordinal\":"<<s.ordinal<<",\"rejection\":"<<s.rejection<<",\"reason\":"<<s.reason<<",\"at\":";array(v.at);
            o<<",\"request\":";array(v.request);o<<",\"plan\":";array(v.plan);o<<",\"active\":";array(v.active);
            o<<",\"accepting\":"<<v.accepting<<",\"fault\":"<<v.fault<<",\"stamp_equal\":"<<v.stamp_equal
             <<",\"pipeline_missing_mask\":"<<v.pipeline_missing_mask<<",\"pipeline_signature_mask\":"<<v.pipeline_signature_mask
             <<",\"stage\":"<<v.stage<<'}';};
        o<<"{\"schema\":"<<schema<<",\"policy\":\"bounded-output-readiness-v"<<schema<<"\",\"diagnostic_only\":true,\"capacity\":8,\"attempts\":"<<attempts_
         <<",\"accepted\":"<<accepted_<<",\"rejected\":"<<rejected_<<",\"reasons\":";array(reasons_);
        o<<",\"baseline\":";if(accepted_)sample(baseline_);else o<<"null";o<<",\"samples\":[";
        for(unsigned i=0;i<first_.size()&&i<rejected_;++i){if(i)o<<',';sample(first_[i]);}
        if(rejected_>first_.size()){o<<',';sample(latest_);}o<<"]}";
    }
};
}

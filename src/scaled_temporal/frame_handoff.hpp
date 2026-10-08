#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace mhw::scaled_temporal {
// A prepared frame's output contract. CPU recording order is deliberately not
// execution order. The caller supplies a qualified, ordered chain of packets;
// this class neither discovers native packets nor proves resource states.
// All operations are serialized by the caller. No waiting or allocation.
class FrameHandoff {
public:
    // TAA, copy, up to 12 effects, 6 DOF passes and 64 late indexed draws.
    static constexpr unsigned capacity=96;
    struct Recording {
        std::uintptr_t list{};
        std::uint64_t generation{};
        bool operator==(const Recording &) const = default;
        explicit operator bool() const noexcept {return list&&generation;}
    };
    struct Identity {
        std::uint64_t frame{},epoch{};
        std::uintptr_t context{},view{};
        bool operator==(const Identity &) const = default;
    };
    enum class Choice:unsigned {undecided,native,reconstructed,spatial};
    enum class Result:unsigned {
        ok,identity,bounds,duplicate,recording_order,already_submitted,
        missing,undecided,submission_order,queue_change,signal,retirement
    };
    struct Use {
        Recording recording;
        std::uint64_t signal{};
        bool reset{};
    };
private:
    Identity identity_{};
    std::array<std::uintptr_t,capacity> packets_{};
    std::array<Use,capacity> uses_{};
    unsigned count_{},recorded_{},submitted_{};
    Choice choice_{};
    std::uintptr_t queue_{};
    std::uint64_t last_signal_{};
public:
    Result prepare(Identity identity,std::span<const std::uintptr_t> packets) noexcept {
        if(count_||!identity.frame||!identity.context||!identity.view)return Result::identity;
        if(packets.empty()||packets.size()>capacity)return Result::bounds;
        for(unsigned i=0;i<packets.size();++i){
            if(!packets[i])return Result::identity;
            for(unsigned j=0;j<i;++j)if(packets[i]==packets[j])return Result::duplicate;
        }
        identity_=identity;count_=static_cast<unsigned>(packets.size());
        for(unsigned i=0;i<count_;++i)packets_[i]=packets[i];
        return Result::ok;
    }
    Result record(unsigned node,std::uintptr_t packet,Recording recording) noexcept {
        if(!count_||node>=count_)return Result::bounds;
        if(packet!=packets_[node]||!recording)return Result::identity;
        if(uses_[node].recording)return Result::duplicate;
        for(unsigned i=0;i<count_;++i)if(uses_[i].recording==recording){
            if(uses_[i].signal||uses_[i].reset)return Result::already_submitted;
            // Commands in one recording cannot be reordered at submission.
            if(i>node)return Result::recording_order;
        }
        // A pointer reused under a different generation cannot belong to this
        // same prepared frame: its earlier recording has already been reset.
        for(unsigned i=0;i<count_;++i)if(uses_[i].recording.list==recording.list&&
            uses_[i].recording.generation!=recording.generation)return Result::identity;
        uses_[node].recording=recording;++recorded_;return Result::ok;
    }
    // Choice is immutable once sealed. For a conditional output implementation
    // it must be published before the first containing GPU submission. Merely
    // constructing this object never authorizes a native or reconstructed draw.
    Result choose(Choice value) noexcept {
        if(!count_||choice_!=Choice::undecided||value==Choice::undecided)return Result::undecided;
        if(submitted_)return Result::already_submitted;
        if(value!=Choice::native&&value!=Choice::reconstructed&&value!=Choice::spatial)return Result::bounds;
        choice_=value;return Result::ok;
    }
    Result validate_submission(std::uintptr_t queue,std::span<const Recording> batch,
                               std::uint64_t signal,unsigned &end) const noexcept {
        end=submitted_;
        if(!count_||!queue)return Result::identity;
        if(choice_==Choice::undecided)return Result::undecided;
        if(!signal||signal==UINT64_MAX||signal<=last_signal_)return Result::signal;
        if(queue_&&queue_!=queue)return Result::queue_change;
        if(batch.empty()||batch.size()>128)return Result::bounds;
        for(unsigned i=0;i<batch.size();++i){
            if(!batch[i])return Result::identity;
            for(unsigned j=0;j<i;++j)if(batch[i].list==batch[j].list)return Result::duplicate;
            for(unsigned node=0;node<count_;++node)if(uses_[node].recording==batch[i]){
                if(uses_[node].signal||uses_[node].reset)return Result::already_submitted;
                if(node!=end)return Result::submission_order;
                ++end;
            }
        }
        if(end==submitted_)return Result::missing;
        // No recorded later node may be hidden in a recording already sent.
        // record() also rejects adding nodes to such recordings afterward.
        return Result::ok;
    }
    Result submitted(std::uintptr_t queue,std::span<const Recording> batch,
                     std::uint64_t signal) noexcept {
        unsigned end{};const auto result=validate_submission(queue,batch,signal,end);
        if(result!=Result::ok)return result;
        for(unsigned i=submitted_;i<end;++i)uses_[i].signal=signal;
        submitted_=end;queue_=queue;last_signal_=signal;return Result::ok;
    }
    // Reset is a successful native Reset receipt, never a request to reset.
    Result reset(Recording recording) noexcept {
        bool found=false;
        for(unsigned i=0;i<count_;++i)if(uses_[i].recording==recording){
            found=true;if(!uses_[i].signal||uses_[i].reset)return Result::retirement;
        }
        if(!found)return Result::missing;
        for(unsigned i=0;i<count_;++i)if(uses_[i].recording==recording)uses_[i].reset=true;
        return Result::ok;
    }
    bool retired(std::uint64_t completed) const noexcept {
        if(!count_||submitted_!=count_||completed==UINT64_MAX)return false;
        for(unsigned i=0;i<count_;++i)if(!uses_[i].reset||uses_[i].signal>completed)return false;
        return true;
    }
    const Identity &identity() const noexcept {return identity_;}
    const std::array<Use,capacity> &uses() const noexcept {return uses_;}
    unsigned count() const noexcept {return count_;}
    unsigned recorded() const noexcept {return recorded_;}
    unsigned submitted_count() const noexcept {return submitted_;}
    Choice choice() const noexcept {return choice_;}
};
}

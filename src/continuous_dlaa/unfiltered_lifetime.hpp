#pragma once
#include "retention.hpp"
#include <span>
namespace mhw::continuous_dlaa {
// Serialized by the runtime mutex. These copies use only current native color
// and output; their lifetime must not depend on the private DLAA depth lease.
class UnfilteredLifetime {
    struct Slot {ID3D12GraphicsCommandList *list{};std::uint64_t generation{};bool submitted{};};
    std::array<Slot,128> slots_{};
    RecordingPins pins_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    std::uint64_t signal_{};
    Slot *find(ID3D12GraphicsCommandList *list,std::uint64_t generation) noexcept {
        for(auto &s:slots_)if(s.list==list&&s.generation==generation)return &s;return nullptr;
    }
public:
    struct Submission {std::array<Slot,128> slots{};unsigned count{};std::uint64_t signal{};};
    bool initialize(ID3D12Device *device) noexcept {return device&&!fence_&&SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)));}
    bool retain(ID3D12GraphicsCommandList *list,std::uint64_t generation,const ResourcePins &resources) noexcept {
        if(!fence_)return false;
        collect();
        if(auto *s=find(list,generation))return !s->submitted&&pins_.matches(list,generation,resources);
        for(auto &s:slots_)if(!s.list){if(!pins_.retain(list,generation,resources))return false;s={list,generation,false};return true;}
        return false;
    }
    bool reset(ID3D12GraphicsCommandList *list,std::uint64_t generation) noexcept {
        if(!pins_.reset(list,generation))return false;
        if(auto *s=find(list,generation))*s={};return true;
    }
    bool tracked(ID3D12GraphicsCommandList *list,std::uint64_t generation) noexcept {return find(list,generation)!=nullptr;}
    bool before(ID3D12CommandQueue *queue,std::span<ID3D12CommandList *const> batch,Submission &token) noexcept {
        token={};
        for(auto *base:batch)for(const auto &s:slots_)if(s.list==base){
            if(s.submitted||token.count==token.slots.size())return false;
            for(unsigned i=0;i<token.count;++i)if(token.slots[i].list==s.list)return false;
            token.slots[token.count++]=s;
        }
        if(!token.count)return true;
        if(!queue||!fence_||queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||signal_==UINT64_MAX-1)return false;
        Microsoft::WRL::ComPtr<ID3D12Device> a,b;
        if(FAILED(queue->GetDevice(IID_PPV_ARGS(&a)))||FAILED(fence_->GetDevice(IID_PPV_ARGS(&b)))||a.Get()!=b.Get())return false;
        // A shared fence on different DIRECT queues needs ordered signals.
        if(queue_&&queue_.Get()!=queue&&FAILED(queue->Wait(fence_.Get(),signal_)))return false;
        token.signal=++signal_;queue_=queue;
        for(unsigned i=0;i<token.count;++i)find(token.slots[i].list,token.slots[i].generation)->submitted=true;
        return true;
    }
    bool after(ID3D12CommandQueue *queue,const Submission &token) noexcept {
        if(!token.count)return true;
        if(FAILED(queue->Signal(fence_.Get(),token.signal)))return false;
        for(unsigned i=0;i<token.count;++i){const auto &s=token.slots[i];const auto *live=find(s.list,s.generation);
            if(!live||!live->submitted||!pins_.submitted(s.list,s.generation,token.signal))return false;
        }
        return true;
    }
    std::uint64_t completed() const noexcept {return fence_?fence_->GetCompletedValue():0;}
    void collect() noexcept {pins_.collect(completed());}
    unsigned live() const noexcept {return pins_.live();}
    ID3D12Fence *fence() const noexcept {return fence_.Get();}
    void write(std::ostream &out,bool collect_completed=true) {
        // GPU progress is independent of the runtime mutex. Collection and all
        // fields in this snapshot must use exactly the same fence observation.
        const auto value=completed();if(collect_completed)pins_.collect(value);
        out<<"{\"schema\":1,\"signal_value\":"<<signal_<<",\"completed_value\":"<<value<<",\"recordings\":";
        pins_.write(out,value);out<<'}';
    }
};
}

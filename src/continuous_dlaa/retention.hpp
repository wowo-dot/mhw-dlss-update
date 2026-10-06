#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <ostream>

namespace mhw::continuous_dlaa {
using ResourcePins = std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,4>;

// Caller serializes access. A GPU fence alone does not retire a recording:
// D3D12 command lists can be submitted again until their next successful Reset.
// Reset alone does not finish GPU work either. Keep both proofs independently.
class RecordingPins {
    struct Entry {
        ID3D12GraphicsCommandList *list{}; // Runtime owns the bounded list registry.
        std::uint64_t generation{}, signal{};
        bool retired{};
        ResourcePins resources;
    };
    std::array<Entry,128> entries_;
    std::uint64_t created_{},submitted_{},retired_{},released_{};
    unsigned live_{},peak_{};
    Entry *find(ID3D12GraphicsCommandList *list,std::uint64_t generation) noexcept {
        for(auto &e:entries_)if(e.list==list&&e.generation==generation)return &e;
        return nullptr;
    }
public:
    bool matches(ID3D12GraphicsCommandList *list,std::uint64_t generation,const ResourcePins &resources) noexcept {
        const auto *e=find(list,generation);if(!e)return false;
        for(unsigned i=0;i<resources.size();++i)if(e->resources[i].Get()!=resources[i].Get())return false;
        return true;
    }
    bool retain(ID3D12GraphicsCommandList *list,std::uint64_t generation,const ResourcePins &resources) noexcept {
        if(!list||!generation||find(list,generation))return false;
        for(const auto &r:resources)if(!r)return false;
        for(auto &e:entries_)if(!e.list){e.list=list;e.generation=generation;e.resources=resources;
            ++created_;++live_;if(live_>peak_)peak_=live_;return true;}
        return false;
    }
    bool submitted(ID3D12GraphicsCommandList *list,std::uint64_t generation,std::uint64_t signal) noexcept {
        auto *e=find(list,generation);
        if(!e||e->signal||e->retired||!signal||signal==UINT64_MAX)return false;
        e->signal=signal;++submitted_;return true;
    }
    bool reset(ID3D12GraphicsCommandList *list,std::uint64_t generation) noexcept {
        auto *e=find(list,generation);if(!e)return true;
        if(!e->signal||e->retired)return false;
        e->retired=true;++retired_;return true;
    }
    void collect(std::uint64_t completed) noexcept {
        // UINT64_MAX means device removal, never successful completion.
        if(completed==UINT64_MAX)return;
        for(auto &e:entries_)if(e.list&&e.retired&&e.signal&&e.signal<=completed){
            e={};--live_;++released_;
        }
    }
    unsigned live() const noexcept {return live_;}
    std::uint64_t released() const noexcept {return released_;}
    void write(std::ostream &out,std::uint64_t completed) const {
        unsigned awaiting_submission{},awaiting_reset{},awaiting_gpu{};
        for(const auto &e:entries_)if(e.list){
            if(!e.signal)++awaiting_submission;
            if(!e.retired)++awaiting_reset;
            if(e.signal&&(completed==UINT64_MAX||e.signal>completed))++awaiting_gpu;
        }
        out<<"{\"schema\":1,\"policy\":\"reset-and-fence-v1\",\"capacity\":128,\"created\":"<<created_
           <<",\"submitted\":"<<submitted_<<",\"retired\":"<<retired_<<",\"released\":"<<released_
           <<",\"live\":"<<live_<<",\"peak\":"<<peak_<<",\"resource_references\":"<<4*live_
           <<",\"awaiting_submission\":"<<awaiting_submission<<",\"awaiting_reset\":"<<awaiting_reset
           <<",\"awaiting_gpu\":"<<awaiting_gpu<<'}';
    }
};
}

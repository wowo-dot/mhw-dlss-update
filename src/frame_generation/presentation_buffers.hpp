#pragma once
#include <array>
#include <cstdint>

namespace mhw::frame_generation {
// CPU metadata for the routed host swap chain. Labels are interface values the
// host received from GetBuffer(ID3D12Resource); they are never dereferenced,
// retained or released here. Equality is identity metadata, not ownership.
struct PresentationBuffers {
    static constexpr unsigned capacity=16,none=UINT32_MAX;
    // A generation starts at chain publication/retirement and every successful
    // resize. Labels from an older generation are never carried forward.
    std::uint64_t swap{},generation{},conflicts{},out_of_range{},foreign{};
    std::array<std::uint64_t,capacity> labels{};
    unsigned known_mask{};
    // Last host GetCurrentBackBufferIndex result in this generation, and the
    // Present count at which it was observed (presents-1 means this frame).
    std::uint64_t game_index_calls{},game_index_present{};
    unsigned game_index{none},game_index_max{none};
    // Bridge query at routed Present entry, before the SDK/native Present.
    std::uint64_t presents{};
    unsigned present_index{none};
    bool label(unsigned index,std::uint64_t &value) const noexcept {
        if(index>=capacity||!(known_mask&(1U<<index)))return false;
        value=labels[index];return true;
    }
    bool contains(std::uint64_t value) const noexcept {
        if(!value)return false;
        for(unsigned i=0;i<capacity;++i)if((known_mask&(1U<<i))&&labels[i]==value)return true;
        return false;
    }
};
// Caller serializes every operation.
class PresentationBufferMap final {
    PresentationBuffers v_;
    void clear(std::uint64_t swap) noexcept {
        const auto generation=v_.generation+1,conflicts=v_.conflicts,range=v_.out_of_range,foreign=v_.foreign;
        v_={};v_.swap=swap;v_.generation=generation;v_.conflicts=conflicts;v_.out_of_range=range;v_.foreign=foreign;
    }
public:
    void chain(std::uint64_t swap) noexcept {clear(swap);}
    void resized(std::uint64_t swap) noexcept {if(!swap||swap!=v_.swap){++v_.foreign;return;}clear(swap);}
    void buffer(std::uint64_t swap,unsigned index,std::uint64_t label) noexcept {
        if(!swap||swap!=v_.swap){++v_.foreign;return;}
        if(index>=PresentationBuffers::capacity){++v_.out_of_range;return;}
        if(!label)return;
        const auto bit=1U<<index;
        // A different label for an index within one generation is unexplained;
        // keep the newest value but make the conflict explicit.
        if((v_.known_mask&bit)&&v_.labels[index]!=label)++v_.conflicts;
        v_.labels[index]=label;v_.known_mask|=bit;
    }
    void game_index(std::uint64_t swap,unsigned index) noexcept {
        if(!swap||swap!=v_.swap){++v_.foreign;return;}
        ++v_.game_index_calls;v_.game_index=index;v_.game_index_present=v_.presents;
        if(v_.game_index_max==PresentationBuffers::none||index>v_.game_index_max)v_.game_index_max=index;
    }
    void present(std::uint64_t swap,unsigned index) noexcept {
        if(!swap||swap!=v_.swap){++v_.foreign;return;}
        ++v_.presents;v_.present_index=index;
    }
    const PresentationBuffers &snapshot() const noexcept {return v_;}
};
}

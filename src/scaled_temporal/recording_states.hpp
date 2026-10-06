#pragma once
#include <array>
#include <cstdint>

namespace mhw::scaled_temporal {
// Observations belong to one Reset generation, never to CPU frame order.
// The caller watches only resources held by prepared plans and serializes use.
// No cross-list inheritance, state inference from bindings, or pointer reads.
template<unsigned ListLimit=128,unsigned ResourceLimit=32,class Owner=std::uintptr_t> class RecordingStates {
public:
    struct State { bool known{}; unsigned value{}; };
    enum class Result { ok, identity, capacity, contradiction };
private:
    struct Resource {std::uintptr_t object{};State state;Owner hold{};};
    struct List {std::uintptr_t object{};std::uint64_t generation{};std::array<Resource,ResourceLimit> resources;};
    std::array<List,ListLimit> lists_{};
public:
    State get(std::uintptr_t list,std::uint64_t generation,std::uintptr_t resource) const noexcept {
        for(const auto &l:lists_)if(l.object==list&&l.generation==generation)
            for(const auto &r:l.resources)if(r.object==resource)return r.state;
        return {};
    }
    Result observe(std::uintptr_t list,std::uint64_t generation,std::uintptr_t resource,
                   bool complete,unsigned before,unsigned after,const Owner &hold={}) noexcept {
        if(!list||!generation||!resource)return Result::identity;
        List *found{},*empty{};
        for(auto &l:lists_){if(l.object==list){if(l.generation!=generation)return Result::identity;found=&l;break;}
            if(!l.object&&!empty)empty=&l;}
        if(!found){if(!empty)return Result::capacity;found=empty;found->object=list;found->generation=generation;}
        Resource *entry{},*vacant{};
        for(auto &r:found->resources){if(r.object==resource){entry=&r;break;}if(!r.object&&!vacant)vacant=&r;}
        if(!entry){if(!vacant)return Result::capacity;entry=vacant;entry->object=resource;entry->hold=hold;}
        if(!complete){entry->state={};return Result::ok;}
        if(entry->state.known&&entry->state.value!=before){entry->state={};return Result::contradiction;}
        entry->state={true,after};return Result::ok;
    }
    bool reset(std::uintptr_t list,std::uint64_t generation) noexcept {
        for(auto &l:lists_)if(l.object==list){if(l.generation!=generation)return false;l={};return true;}
        return true;
    }
    void clear() noexcept {for(auto &l:lists_)l={};}
};
}

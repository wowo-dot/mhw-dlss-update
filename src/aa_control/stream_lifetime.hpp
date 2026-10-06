#pragma once
#include <cstdint>
namespace mhw::aa_control {
// Serialized caller. A reusable marker remains immutable until its CPU producer
// and consumer returned, its native recording reset, AND its GPU fence completed.
struct StreamLifetime {
    enum Stage {free,projected,queued,marked,recorded,submitted};
    Stage stage{free};std::uint64_t frame{},generation{},signal{};
    bool producer_returned{},consumer_returned{},retired{},signal_confirmed{};
    bool reserve(std::uint64_t f){if(stage!=free||!f)return false;*this={};stage=projected;frame=f;return true;}
    bool queue(){if(stage!=projected)return false;stage=queued;return true;}
    bool mark(std::uint64_t g){if(stage!=queued||!g)return false;generation=g;stage=marked;return true;}
    bool record(){if(stage!=marked||!consumer_returned)return false;stage=recorded;return true;}
    bool submit(std::uint64_t g,std::uint64_t s){if(stage!=recorded||g!=generation||!s||s==UINT64_MAX)return false;signal=s;stage=submitted;return true;}
    bool confirm(std::uint64_t s){if(stage!=submitted||signal!=s||signal_confirmed)return false;signal_confirmed=true;return true;}
    bool reset(std::uint64_t g){if(stage!=submitted||generation!=g||retired)return false;retired=true;return true;}
    bool recyclable(std::uint64_t completed) const {return stage==submitted&&signal_confirmed&&retired&&producer_returned&&consumer_returned&&completed!=UINT64_MAX&&completed>=signal;}
};
}

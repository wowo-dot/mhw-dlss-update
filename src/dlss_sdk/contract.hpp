#pragma once
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
namespace mhw::dlss_sdk {
struct Frame {unsigned width{},height{};std::span<const float> color,depth,motion;};
inline bool validate(const Frame &f,std::string &reason) {
    const auto n=std::uint64_t(f.width)*f.height;
    if(f.width<32 || f.height<32 || f.width>8192 || f.height>8192 || n>4194304 ||
        f.color.size()!=n*4 || f.depth.size()!=n || f.motion.size()!=n*2) {
        reason="invalid DLAA input extent";return false;
    }
    for(std::size_t i=0;i<n;++i) {
        for(unsigned k=0;k<3;++k)if(!std::isfinite(f.color[i*4+k]) || f.color[i*4+k]<0) {
            reason="invalid linear color";return false;
        }
        if(f.color[i*4+3]!=1 || !std::isfinite(f.depth[i]) || f.depth[i]<0 || f.depth[i]>1 ||
            !std::isfinite(f.motion[i*2]) || !std::isfinite(f.motion[i*2+1])) {
            reason="invalid alpha/depth/motion";return false;
        }
    }
    reason.clear();return true;
}
}

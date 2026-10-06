#include "provenance.hpp"
namespace mhw::projection_binding::provenance {
bool destination_ranges(unsigned count,const std::uintptr_t *starts,const unsigned *sizes,unsigned stride,
                        std::array<Range,range_capacity> &out) noexcept {
  if(count>range_capacity || (count && !starts) || !stride)return false;
  out={};
  for(unsigned i=0;i<count;++i) {
    const auto n=sizes?sizes[i]:1U;
    if(!starts[i] || !n || n>65536 || starts[i]>UINTPTR_MAX-static_cast<std::uint64_t>(n)*stride)return false;
    out[i]={starts[i],n};
  }
  return true;
}
bool valid_depth_view(const std::array<unsigned,6> &d,const ResourceDescription &r,unsigned &subresource) noexcept {
  // First supported path: D32_FLOAT, one plane, one sample, writable depth.
  // Other formats/flags are retained as evidence, never silently normalized.
  if(!r.valid || r.dimension!=3 || r.format!=39 || !(r.flags&2) || r.samples!=1 ||
     !r.mips || !r.depth_or_array || d[0]!=40 || d[2]!=0)return false;
  if(d[1]==3) {
    if(r.depth_or_array!=1 || d[3]>=r.mips)return false;
    subresource=d[3];return true;
  }
  if(d[1]!=4 || d[3]>=r.mips || d[4]>=r.depth_or_array || d[5]!=1)return false;
  subresource=d[3]+d[4]*r.mips;return true;
}
}

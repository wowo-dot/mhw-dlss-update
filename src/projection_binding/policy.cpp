#include "recorder.hpp"
#include <cstring>
#include <limits>

namespace mhw::projection_binding {
Location locate(const Arena &a,std::uintptr_t pointer,unsigned bytes) noexcept {
  Location out{};
  if (!pointer || (pointer&255) || !bytes || bytes>max_constants || a.mode>2 || a.index>=3 ||
      !a.stride || (a.stride&255) || a.stride>0x10000000 || bytes>UINT32_MAX-255) return out;
  const auto extent=(bytes+255)&~255U;
  if(a.cpu[0]>UINTPTR_MAX-static_cast<std::uint64_t>(a.stride)*3 ||
     a.gpu[0]>UINT64_MAX-static_cast<std::uint64_t>(a.stride)*3) return out;
  unsigned matches{};
  for(unsigned i=0;i<3;++i) {
    if (!a.cpu[i] || !a.gpu[i] || (a.cpu[i]&255) || (a.gpu[i]&255) ||
        a.used[i]>a.stride || a.cpu[i]>UINTPTR_MAX-a.stride || a.gpu[i]>UINT64_MAX-a.stride) return {};
    if (i && (a.cpu[i]!=a.cpu[0]+static_cast<std::uint64_t>(a.stride)*i ||
              a.gpu[i]!=a.gpu[0]+static_cast<std::uint64_t>(a.stride)*i)) return {};
    if (pointer>=a.cpu[i] && pointer-a.cpu[i]<a.stride) {
      const auto offset=static_cast<unsigned>(pointer-a.cpu[i]);
      if (extent>a.stride-offset || offset>a.used[i] || extent>a.used[i]-offset) return {};
      out={true,i,offset,extent,a.gpu[i]+offset}; ++matches;
    }
  }
  return matches==1?out:Location{};
}
bool dxbc_valid(const std::byte *bytes,unsigned size) noexcept {
  if (!bytes || size<32 || size>max_shader || std::memcmp(bytes,"DXBC",4)) return false;
  const auto word=[&](unsigned off){unsigned v{};std::memcpy(&v,bytes+off,4);return v;};
  if(word(20)!=1 || word(24)!=size) return false;
  const auto count=word(28);
  if(!count || count>64 || count>(size-32)/4) return false;
  const auto header=32+count*4;
  for(unsigned i=0;i<count;++i) {
    const auto at=word(32+4*i);
    if(at<header || at>size-8 || word(at+4)>size-at-8) return false;
    const auto end=at+8+word(at+4);
    for(unsigned j=0;j<i;++j) {const auto prior=word(32+4*j),last=prior+8+word(prior+4);if(at<last && prior<end)return false;}
  }
  return true;
}
}

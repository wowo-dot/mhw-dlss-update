#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace mhw::scaled_temporal {
struct ColorLink {unsigned key{};std::uint64_t source{},destination{};};
// Follow unique native color-owner edges. Shader qualification and GPU
// ordering remain separate requirements. Never infer an edge from a name.
inline bool color_chain(std::span<const ColorLink> links,std::uint64_t source,
                        std::uint64_t destination,std::array<unsigned,8> &path,unsigned &count) noexcept {
 path={};count=0;if(!source||!destination||links.size()>16)return false;
 std::array<unsigned,8> result{};unsigned n{},previous=0x78000064;
 std::array<std::uint64_t,9> visited{};visited[0]=source;
 while(source!=destination){
  unsigned match=UINT32_MAX;
  for(unsigned i=0;i<links.size();++i)if(links[i].source==source){
   if(match!=UINT32_MAX)return false;match=i;
  }
  if(match==UINT32_MAX||n==result.size())return false;const auto &l=links[match];
  if(!l.destination||l.key<=previous||l.key>=0x7ffffff1)return false;
  for(unsigned i=0;i<=n;++i)if(visited[i]==l.destination)return false;
  result[n++]=match;visited[n]=l.destination;source=l.destination;previous=l.key;
 }
 path=result;count=n;return true;
}
}

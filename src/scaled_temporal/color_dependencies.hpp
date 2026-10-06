#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace mhw::scaled_temporal::color_dependencies {
// CPU plan only. IDs identify already-qualified resources, not addresses to
// dereference. Caller separately proves GPU identity, states and lifetimes.
// "Scene" describes dependency provenance, not shader semantics, coverage,
// allocation dimensions, color space, or permission to execute a replacement.
enum class Domain { unknown, scene, auxiliary };
struct Resource { std::uint64_t id{}; Domain domain{}; };
struct Draw {
 std::uint64_t output{};
 std::array<std::uint64_t,8> reads{};
 unsigned count{};
};
struct Input { Domain domain{}; int producer{-1}; };
struct Step { Domain output{}; std::array<Input,8> inputs{}; };
struct Plan { std::array<Step,16> steps{}; unsigned count{}; };
// Resolve latest writer in native order. A temporary becomes scene-derived
// when any input is scene-derived; auxiliary inputs retain their own domains.
// Unknown reads, SRV/RTV self-aliases and declared auxiliary writes reject the
// whole plan. No partial result is published.
inline bool build(std::span<const Resource> resources,std::span<const Draw> draws,
                  std::uint64_t final_output,Plan &out) noexcept {
 out={};if(resources.empty()||resources.size()>32||draws.empty()||draws.size()>16)return false;
 std::array<Domain,32> domains{};std::array<int,32> writers{};writers.fill(-1);
 const auto find=[&](std::uint64_t id){for(unsigned i=0;i<resources.size();++i)if(resources[i].id==id)return int(i);return -1;};
 for(unsigned i=0;i<resources.size();++i){const auto &r=resources[i];
  if(!r.id||find(r.id)!=int(i)||(r.domain!=Domain::unknown&&r.domain!=Domain::scene&&r.domain!=Domain::auxiliary))return false;
  domains[i]=r.domain;
 }
 const int final=find(final_output);if(final<0)return false;Plan next;
 for(unsigned i=0;i<draws.size();++i){const auto &d=draws[i];const int target=find(d.output);
  if(target<0||!d.count||d.count>d.reads.size()||resources[target].domain==Domain::auxiliary)return false;
  auto &step=next.steps[i];step.output=Domain::auxiliary;
  for(unsigned j=0;j<d.count;++j){const int source=find(d.reads[j]);
   if(source<0||source==target||domains[source]==Domain::unknown)return false;
   step.inputs[j]={domains[source],writers[source]};
   if(domains[source]==Domain::scene)step.output=Domain::scene;
  }
  // This component routes scene-dependent work, not unrelated auxiliary jobs.
  if(step.output!=Domain::scene)return false;
  domains[target]=step.output;writers[target]=int(i);++next.count;
 }
 if(domains[final]!=Domain::scene||writers[final]!=int(draws.size()-1))return false;
 out=next;return true;
}
}

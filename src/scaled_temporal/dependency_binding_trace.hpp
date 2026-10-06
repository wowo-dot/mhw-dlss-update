#pragma once
#include <array>
#include <cstdint>
#include <ostream>
namespace mhw::scaled_temporal {
// Bounded diagnostic data only. Never changes binding admission or rendering.
struct BindingReadProgress {unsigned phase{},index{};};
struct BindingCheck {
 bool observed{},readable{},contract{},equal{};
 BindingReadProgress read;
 unsigned fields{},differences{},stored{};
 std::array<std::array<std::uint64_t,4>,16> changes{};
 void difference(unsigned field,unsigned index,std::uint64_t before,std::uint64_t after) noexcept {
  if(before==after)return;
  fields|=1U<<field;++differences;
  if(stored<changes.size())changes[stored++]={field,index,before,after};
 }
 bool accepted() const noexcept{return observed&&readable&&contract&&equal;}
 void write(std::ostream &o) const {
  o<<"{\"observed\":"<<observed<<",\"readable\":"<<readable<<",\"contract\":"<<contract<<",\"equal\":"<<equal
   <<",\"read\":["<<read.phase<<','<<read.index<<"],\"fields\":"<<fields<<",\"differences\":"<<differences<<",\"changes\":[";
  for(unsigned i=0;i<stored;++i){if(i)o<<',';o<<'[';for(unsigned j=0;j<4;++j){if(j)o<<',';o<<changes[i][j];}o<<']';}o<<"]}";
 }
};
struct DependencyBindingTrace {
 std::uint64_t entry_checks{},draw_checks{},entry_rejections{},draw_rejections{};
 struct Sample {
  // tick, frame, epoch, stage, dependency kind, packet, graphics, shader set
  std::array<std::uint64_t,8> at{};
  BindingCheck entry,draw;
 };
 std::array<Sample,3> samples{};unsigned count{};
 void entry(const BindingCheck &c) noexcept{++entry_checks;entry_rejections+=!c.accepted();}
 void draw(const std::array<std::uint64_t,8> &at,const BindingCheck &entry,const BindingCheck &current) noexcept {
  ++draw_checks;if(current.accepted())return;++draw_rejections;
  if(count<samples.size())samples[count++]={at,entry,current};
 }
 void write(std::ostream &o) const {
  o<<"{\"schema\":2,\"policy\":\"dependency-binding-boundaries-v2\",\"diagnostic_only\":true,\"capacity\":3,\"entry_checks\":"<<entry_checks
   <<",\"draw_checks\":"<<draw_checks<<",\"entry_rejections\":"<<entry_rejections<<",\"draw_rejections\":"<<draw_rejections<<",\"samples\":[";
  for(unsigned i=0;i<count;++i){if(i)o<<',';const auto &s=samples[i];o<<"{\"at\":[";for(unsigned j=0;j<s.at.size();++j){if(j)o<<',';o<<s.at[j];}
   o<<"],\"entry\":";s.entry.write(o);o<<",\"draw\":";s.draw.write(o);o<<'}';}o<<"]}";
 }
};
}

#pragma once
#include "recording_states.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace mhw::scaled_temporal {
// Diagnostic state only. No state is inherited across lists/generations, and
// no result authorizes a barrier, dispatch, binding change or resource copy.
class ComputeResourceTrace {
public:
 static constexpr unsigned resource_limit=32,event_limit=256;
 struct Event {std::uint64_t list{},generation{},resource{};unsigned kind{},flags{},subresource{},before{},after{},result{};};
 using Journal=RecordingStates<128,resource_limit>;
private:
 Journal journal_;
 std::array<std::uint64_t,resource_limit> resources_{};
 std::array<Event,event_limit> events_{};
 unsigned resources_count_{},events_count_{},lost_{};
 bool stopped_{};
public:
 bool watch(std::uint64_t resource) noexcept {
  if(!resource||stopped_)return false;
  for(unsigned i=0;i<resources_count_;++i)if(resources_[i]==resource)return true;
  if(resources_count_==resource_limit)return false;
  resources_[resources_count_++]=resource;return true;
 }
 bool watched(std::uint64_t resource)const noexcept {
  for(unsigned i=0;i<resources_count_;++i)if(resources_[i]==resource)return true;
  return false;
 }
 void observe(Event e,bool complete) noexcept {
  if(stopped_||!watched(e.resource))return;
  const auto r=journal_.observe(e.list,e.generation,e.resource,complete,e.before,e.after);
  e.result=static_cast<unsigned>(r);
  if(r==Journal::Result::identity||r==Journal::Result::capacity){++lost_;stopped_=true;return;}
  if(events_count_==event_limit){++lost_;stopped_=true;return;}
  events_[events_count_++]=e;
 }
 Journal::State get(std::uint64_t list,std::uint64_t generation,std::uint64_t resource)const noexcept {
  return stopped_?Journal::State{}:journal_.get(list,generation,resource);
 }
 void reset(std::uint64_t list,std::uint64_t generation)noexcept {
  if(!journal_.reset(list,generation)){++lost_;stopped_=true;}
 }
 void stop()noexcept{stopped_=true;journal_.clear();}
 bool stopped()const noexcept{return stopped_;}
 unsigned lost()const noexcept{return lost_;}
 std::span<const Event> events()const noexcept{return {events_.data(),events_count_};}
};
}

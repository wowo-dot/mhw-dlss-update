#pragma once
// v0.26.0: a completed post queue builds the frame's identity plan once, on the
// render thread that published the frame (TAA's producer, inside TAA's
// callback), for that frame's manager, context, view and TAA layer, while its
// stamp holds. A scaled frame full_post did not arm is not planned.
__declspec(noinline) void check_queue_completion(const mhw::temporal_bindings::Snapshot &s,std::uintptr_t filters,std::uintptr_t final_copy,unsigned count){
 namespace route=mhw::scaled_temporal::stream_route;
 using mhw::fixtures::effects::require;
 const auto published=route::published,built=route::built,unqualified=route::unqualified;
 const auto slot=s.frame%route::plans.size();const auto saved=std::make_unique<route::Plan>(route::plans[slot]);
 // The final copy after the native queue; the frame armed by full_post.
 std::uintptr_t queue{};require(route::field(s.context,0x2b78,queue),"fixture queue");
 put(queue+16*count,0x80000000U);put(queue+16*count+8,final_copy);put(s.context+0x2b80,count+1);
 route::full_frame.store(s.frame);
 const auto complete=[&]{route::post_queue_complete(filters,s.context,s.view,s.layer);};
 const auto missing=[&]{require(!route::plans[slot].built,"unqualified completion cannot build");};
 route::publish(s,{});
 route::post_queue_complete(filters+8,s.context,s.view,s.layer);missing();
 route::post_queue_complete(filters,s.context+8,s.view,s.layer);missing();
 route::post_queue_complete(filters,s.context,s.view+8,s.layer);missing();
 route::post_queue_complete(filters,s.context,s.view,s.layer+8);missing();
 {std::thread foreign(complete);foreign.join();missing();}
 put(route::cfg.renderer+0x1f208,s.epoch+1);complete();missing();put(route::cfg.renderer+0x1f208,s.epoch);
 complete();require(route::plans[slot].built&&route::plans[slot].identity&&route::built==built+1,"the identity plan built once the queue completes");
 complete();require(route::built==built+1,"duplicate queue completion is inert");
 route::publish(s,{});route::accepting=false;complete();missing();route::accepting=true;
 {std::thread other([&]{route::publish(s,{});});other.join();complete();missing();}
 {route::publish(s,{});route::full_frame.store(UINT64_MAX);const auto unarmed=route::unarmed;complete();missing();
  require(route::unarmed==unarmed+1&&route::unqualified==unqualified+1,"an unarmed scaled frame is counted, not planned");route::full_frame.store(s.frame);}
 {auto newer=s;newer.frame+=32;++newer.epoch;route::publish(newer,{});complete();missing();}
 route::publish(s,{});complete();require(route::plans[slot].built,"the next valid queue recovers");
 put(s.context+0x2b80,count);put(queue+16*count,0U);put(queue+16*count+8,std::uintptr_t{});route::full_frame.store(UINT64_MAX);
 route::plans[slot]=*saved;route::published=published;route::built=built;route::unqualified=unqualified;
 require(route::healthy(),"queue timing rejections do not latch runtime fault");
 std::cout<<"PASS completed-queue timing: manager/context/view/layer, publishing thread, epoch, stop, duplicate, unarmed and stale slot\n";
}

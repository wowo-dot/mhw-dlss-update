#pragma once
__declspec(noinline) void check_queue_completion(const mhw::temporal_bindings::Snapshot &s,std::uintptr_t filters,std::uintptr_t motion,unsigned count){
 namespace route=mhw::scaled_temporal::stream_route;
 using mhw::fixtures::effects::require;
 const auto published=route::published,built=route::built,unqualified=route::unqualified;
 const auto slot=s.frame%route::plans.size();const auto saved=std::make_unique<route::Plan>(route::plans[slot]);
 mhw::projection_binding::Frame f;f.caller=route::cfg.image+0x24f228b;f.gpr[1]=motion;f.gpr[2]=s.context;
 const auto complete=[&]{route::post_queue_complete(filters,s.context,s.view,s.layer);};
 const auto enter=[&]{route::publish(s,{});return route::post_before(f);};
 const auto missing=[&]{require(!route::plans[slot].built,"unqualified completion cannot build");};
 // Native enqueuing reaches motion before the last filter (haze when present).
 auto receipt=enter();put(s.context+0x2b80,count-1);
 route::post_after(receipt,true);missing();
 require(route::unqualified==unqualified,"partial queue is not rejected at motion return");
 put(s.context+0x2b80,count);
 route::post_queue_complete(filters+8,s.context,s.view,s.layer);missing();
 route::post_queue_complete(filters,s.context+8,s.view,s.layer);missing();
 route::post_queue_complete(filters,s.context,s.view+8,s.layer);missing();
 route::post_queue_complete(filters,s.context,s.view,s.layer+8);missing();
 std::thread foreign(complete);foreign.join();missing();
 put(route::cfg.renderer+0x1f208,s.epoch+1);complete();missing();put(route::cfg.renderer+0x1f208,s.epoch);
 complete();require(route::plans[slot].built&&route::built==built+1,"complete queue admitted after all filter draws");
 complete();require(route::built==built+1,"duplicate queue completion is inert");
 receipt=enter();route::post_after(receipt,false);complete();missing();
 receipt=enter();route::post_after(receipt,true);route::accepting=false;complete();missing();route::accepting=true;
 receipt=enter();route::post_after(receipt,true);const auto duplicate=route::post_before(f);route::post_after(duplicate,true);complete();missing();
 receipt=enter();auto newer=s;newer.frame+=32;++newer.epoch;
 put(route::cfg.renderer+0x1f210,newer.frame);put(route::cfg.renderer+0x1f208,newer.epoch);
 route::publish(newer,{});route::post_after(receipt,true);complete();missing();
 require(!route::plans[slot].motion_returned,"stale motion return cannot authorize reused plan slot");
 put(route::cfg.renderer+0x1f210,s.frame);put(route::cfg.renderer+0x1f208,s.epoch);
 receipt=enter();route::post_after(receipt,true);complete();require(route::plans[slot].built,"next valid queue recovers");
 route::plans[slot]=*saved;route::published=published;route::built=built;route::unqualified=unqualified;
 require(route::healthy(),"queue timing rejections do not latch runtime fault");
 std::cout<<"PASS completed-queue timing: partial queue, identity/thread/epoch, stop, exception, duplicate and stale return\n";
}

#pragma once
// Real route entry/recording bookkeeping with owned D3D12 objects. Unknown
// material remains native, but its selector and GPU-order node must retire.
void check_indexed_ownership(fx::Fixture &f){
 using namespace fx;using H=sf::FrameHandoff;
 auto vertices=sf::effect_vertices(sf::EffectStage::motion_filter,{64,32,128,64},false);
 auto rec=recipe(sf::EffectStage::motion_filter,{64,32,128,64},f.root.Get(),f.vs[0],f.ps[0],std::span(vertices.values).first(16));
 auto pipeline=fx::pipeline(f.device.Get(),rec,f.ps[0]);
 hr(f.allocator->Reset(),"indexed allocator");hr(f.list->Reset(f.allocator.Get(),pipeline.Get()),"indexed list");
 obs::reset(f.list.Get(),7,pipeline.Get());f.list->SetGraphicsRootSignature(f.root.Get());
 std::array<std::array<std::byte,80>,2> packets{};std::array<std::byte,0x160> recording{};std::array<std::byte,16> executor{};
 put(executor,8,reinterpret_cast<std::uintptr_t>(recording.data()));put(recording,0x20,reinterpret_cast<std::uintptr_t>(f.list.Get()));
 put(recording,0x108,reinterpret_cast<std::uintptr_t>(pipeline.Get()));put(recording,0x110,reinterpret_cast<std::uintptr_t>(f.root.Get()));
 auto &p=route::plans[1];p.stamp.frame=1;p.stamp.epoch=1;p.stamp.context=2;p.stamp.view=3;
 p.built=p.prepared=p.claimed=true;p.late_count=2;
 auto pin=buffer(f.device.Get(),256,D3D12_HEAP_TYPE_UPLOAD);for(auto &resource:p.resources)resource=pin;
 for(unsigned i=0;i<2;++i){p.late_packets[i]=reinterpret_cast<std::uintptr_t>(packets[i].data());p.late_keys[i]=0x7ffffff4+i;
  p.late[i].arguments={6,3,static_cast<unsigned>(-4)};put(packets[i],0x40,p.late[i].arguments);}
 route::watch(1,p);
 // Resolve, copy, motion, grade, two silhouettes, final screen.
 const std::array<std::uintptr_t,7> nodes{11,12,13,14,p.late_packets[0],p.late_packets[1],15};
 require(route::handoff_indexed(p,0)==4&&route::handoff_indexed(p,1)==5&&route::handoff_draw(p,2)==6,"late indexed order before final");
 require(route::prepare_selector(p)&&p.handoff.prepare({1,1,2,3},nodes)==H::Result::ok&&route::choose(p,H::Choice::reconstructed),"indexed prepared selector");
 const H::Recording identity{reinterpret_cast<std::uintptr_t>(f.list.Get()),7};
 for(unsigned node=0;node<4;++node)require(p.handoff.record(node,nodes[node],identity)==H::Result::ok,"earlier native nodes");
 route::silhouette_planned=2;
 for(unsigned i=0;i<2;++i){
  mhw::projection_binding::Frame frame{};frame.gpr[1]=reinterpret_cast<std::uintptr_t>(executor.data());frame.gpr[2]=4;frame.gpr[3]=p.late_packets[i];
  route::indexed_scope=route::indexed_before(frame);put(recording,0x158,p.late_packets[i]);
  require(route::indexed_scope.slot==1&&route::indexed_scope.position==i,"actual indexed entry matched frozen packet");
  require(!route::indexed_draw(f.list.Get(),6,1,3,-4,0,route::cfg.image+0x259d806),"unknown material forwards unchanged");
  route::silhouette_after(true);route::indexed_scope={};
 }
 require(route::healthy()&&route::silhouette_recorded==2&&route::silhouette_native==2&&!route::silhouette_corrected,"native indexed receipts");
 require(p.handoff.record(6,15,identity)==H::Result::ok,"final follows indexed draws");
 const std::array<std::array<std::uint64_t,2>,1> batch{{{identity.list,identity.generation}}};
 require(route::submission_needed(batch)&&route::validate_submission(reinterpret_cast<std::uintptr_t>(f.queue.Get()),batch,1),"indexed retained recording needs signal");
 hr(f.list->Close(),"indexed close");ID3D12CommandList *lists[]{f.list.Get()};f.queue->ExecuteCommandLists(1,lists);f.wait();
 require(route::submitted_batch(reinterpret_cast<std::uintptr_t>(f.queue.Get()),batch,1),"actual indexed route submission");
 route::collect(1);require(route::lifetime.live()==1&&p.prepared,"completion alone retains indexed selector");
 hr(f.allocator->Reset(),"indexed retire allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"indexed retire list");
 require(route::reset(f.list.Get(),7),"indexed successful Reset receipt");route::collect(1);
 require(!route::lifetime.live()&&!p.prepared&&p.handoff.retired(1),"indexed resources and frame retire together");
 hr(f.list->Close(),"indexed retire close");f.messages();
 std::cout<<"PASS actual indexed route: native forwarding, seven-node order, GPU submission and Reset/completion retention\n";
}

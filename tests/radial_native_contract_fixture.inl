// Captured combat contracts: a default mask shared with occlusion, and two
// source-free radial descriptors under one native sort key. Real GPU resources
// back synthetic native packets; admission calls the production graph builder.
{
 put(graphics_objects[1]+0x58,extra_owners[1]);
 auto defaults=accept_dependency(2);
 require(defaults->draws[5].reads[1]==defaults->draws[5].reads[2],"native default t1/t2 resource alias retained once");
 require(defaults->dependency_bindings[1].views[1]==defaults->dependency_bindings[1].views[2],"native default mask keeps same view");
 require(route::dependency_enter(*defaults,5)&&route::dependency_current_stable(*defaults,5),"default mask entry and API contract");
 put(graphics_objects[1]+0x58,extra_owners[2]);
 require(!route::dependency_enter(*defaults,5),"mid-plan mask owner replacement rejected");
 auto custom=accept_dependency();
 require(route::dependency_enter(*custom,5)&&route::dependency_current_stable(*custom,5),"fresh custom mask plan");
 put(graphics_objects[1]+0x58,extra_owners[1]);
 require(!route::dependency_enter(*custom,5),"mid-plan default owner replacement rejected");
 defaults=accept_dependency(2);
 require(route::dependency_enter(*defaults,5)&&route::dependency_current_stable(*defaults,5),"fresh default mask plan");
 // Reject actual integer storage even with the admitted native format tag.
 const auto bad_owner=a.take(0x100),bad_backend=a.take(0x40);
 std::memcpy(reinterpret_cast<void *>(bad_owner),reinterpret_cast<void *>(extra_owners[1]),0x100);
 auto integer=resource(f.device.Get(),texture(1,1,DXGI_FORMAT_R8G8B8A8_UINT),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
 put(bad_owner+0x60,bad_backend);put(bad_backend+0x20,reinterpret_cast<std::uintptr_t>(integer.Get()));
 put(graphics_objects[1]+0x58,bad_owner);
 mhw::scaled_temporal::OutputDiagnostics::Graph radial_observation;
 const auto radial_build=[&](route::Plan &plan){radial_observation={};return route::build_plan_observed(plan,motion,radial_observation);};
 auto radial_rejected=std::make_unique<route::Plan>();radial_rejected->stamp=s;
 require(!radial_build(*radial_rejected)&&route::dependency_rejection==3&&route::dependency_stage==5,"integer default mask rejected at exact stage");
 require(radial_observation.command_gate[0]==mhw::scaled_temporal::OutputCommands::input_bindings&&
         radial_observation.command_gate[1]==0x78001004&&radial_observation.command_gate[2]==dependency_packets[1],"failed dependency key and packet preserved");
 put(bad_backend+0x20,reinterpret_cast<std::uintptr_t>(extra[1].Get()));put(bad_owner+0x90,8U);
 radial_rejected=std::make_unique<route::Plan>();radial_rejected->stamp=s;
 require(!radial_build(*radial_rejected)&&route::dependency_rejection==3,"unknown native mask format rejected");
 put(graphics_objects[1]+0x58,extra_owners[2]);accept_dependency();

 const auto second_effect=a.take(0x200);
 std::memcpy(reinterpret_cast<void *>(second_effect),reinterpret_cast<void *>(effect),0x200);
 put(second_effect+0x148,second_effect);put(filters+0x68,second_effect+0x140);put(filters+0x3c,6U);
 const auto state=a.take(0x50);put(state,11U);
 std::array<std::uintptr_t,2> clones{};
 for(unsigned i=0;i<2;++i){clones[i]=a.take(0x50);std::memcpy(reinterpret_cast<void *>(clones[i]),reinterpret_cast<void *>(dependency_packets[i]),0x50);}
 // Higher-address pair first; descriptor order cannot choose packet order.
 const std::array<std::uintptr_t,8> sequence{state,clones[0],state,clones[1],state,dependency_packets[0],state,dependency_packets[1]};
 for(unsigned i=0;i<sequence.size();++i){put(queue+16*(4+i),0x78001004U);put(queue+16*(4+i)+8,sequence[i]);}
 put(context+0x2b80,12U);
 const auto accept_pairs=[&]{auto q=std::make_unique<route::Plan>();q->stamp=s;
  require(route::build_plan(*q,motion)&&q->overlay_count==4&&q->extra_resources==3,"equivalent-key repeated radial pairs admitted");
  for(unsigned i=0;i<4;++i){require(q->order[1+i]==4+i&&q->draws[4+i].draw.packet==sequence[1+2*i],"native stable draw order preserved");
   require(q->draws[4+i].dependency==(i%2?2U:1U),"every copy and radial independently qualified");}
  require(route::dependency_order(*q),"latest scene writer feeds following radial pair");return q;};
 accept_pairs();
 put(filters+0x60,second_effect+0x140);put(filters+0x68,effect+0x140);accept_pairs();
 put(second_effect+0x158,owners[3]);radial_rejected=std::make_unique<route::Plan>();radial_rejected->stamp=s;
 require(!radial_build(*radial_rejected)&&radial_observation.command_gate[0]==mhw::scaled_temporal::OutputCommands::ambiguous_key,"conflicting equal-key outputs remain rejected");
 put(second_effect+0x158,owners[2]);
 put(queue+16*11+8,clones[1]);radial_rejected=std::make_unique<route::Plan>();radial_rejected->stamp=s;
 require(!radial_build(*radial_rejected)&&radial_observation.command_gate[0]==mhw::scaled_temporal::OutputCommands::duplicate_packet,"aliased descriptors do not authorize duplicate packets");
 put(queue+16*11+8,dependency_packets[1]);accept_pairs();
 put(filters+0x60,effect+0x140);put(filters+0x3c,5U);
 put(queue+64,0x78001004U);put(queue+72,dependency_packets[0]);put(queue+80,0x78001004U);put(queue+88,dependency_packets[1]);put(context+0x2b80,6U);
 accept_dependency();route::dependency_binding_trace={};route::dependency_refresh={};
 std::cout<<"PASS default/custom fresh plans, owner-change/integer/unknown-format rejection, equivalent-key radial pairs, stable order, conflicting target and duplicate packet rejection\n";
}

#pragma once
void native_state_fixture(){
    Block renderer(0x7af40),schedule(16*70),packets(0x30*70),backings(0x80*3),trackers(0x38*3),spans(0x50),node(0x20);
    tb::Snapshot source;source.context=renderer.p+0x710;source.frame=source.epoch=9;
    put(renderer.p+0x1f208,std::uint64_t{9});put(renderer.p+0x7af18,schedule.p);put(renderer.p+0x7af28,3U);
    for(unsigned i=0;i<70;++i){put(schedule.p+16*i+8,packets.p+0x30*i);put(packets.p+0x30*i,6U);}
    constexpr unsigned roles[]{0,4,5};
    for(unsigned i=0;i<3;++i){auto &owner=source.inputs[roles[i]];owner.backing=backings.p+0x80*i;owner.resource=0x1000+0x1000*i;
        const auto tracker=trackers.p+0x38*i;put(owner.backing+0x20,owner.resource);put(owner.backing+0x70,tracker);
        put(tracker+0x10,std::uint64_t{9});put(tracker+0x18,std::uint16_t{4});put(tracker+0x1a,std::uint16_t{0x100});put(tracker+0x28,static_cast<unsigned char>(1));}
    put(trackers.p,spans.p);put(trackers.p+8,spans.p+0x28);
    put(spans.p,packets.p);put(spans.p+8,2U);put(spans.p+0x10,spans.p+0x28);put(spans.p+0x20,std::uint16_t{0x100});put(spans.p+0x22,std::uint16_t{4});
    put(spans.p+0x28,packets.p+0x60);put(spans.p+0x40,spans.p);put(spans.p+0x48,std::uint16_t{4});put(spans.p+0x4a,std::uint16_t{0x100});
    put(packets.p+8,node.p);put(node.p+8,backings.p);put(node.p+0x10,spans.p);put(node.p+0x18,std::uint16_t{0x100});put(node.p+0x1a,std::uint16_t{4});
    const auto snapshot=[&](const char *name,const tb::Reader &reader=cd::reader){auto result=std::make_unique<cd::NativeStateTrace>();result->capture(reader,source,packets.p+0x60);
        std::cout<<std::boolalpha<<"{\"case\":\""<<name<<"\",\"provenance\":";result->write(std::cout);std::cout<<"}\n";return result;};
    auto a=snapshot("earlier-transition");
    check(a->stable&&a->scan_complete&&a->index==2&&a->matches==1&&a->earlier_count==2,"located scheduled packet");
    check(a->earlier[0].complete&&a->earlier[0].count==1&&a->earlier[0].nodes[0][1]==1,"earlier color transition retained");
    check(a->trackers[0].complete&&a->trackers[0].stable&&a->trackers[0].count==2&&a->trackers[0].final==0x100&&a->trackers[0].spans[0][6]==4,"planned final state is not packet state");
    put(schedule.p+8,packets.p+0x60);a=snapshot("duplicate-packet");check(a->matches==2&&!a->earlier_count,"ambiguous packet no earlier association");put(schedule.p+8,packets.p);
    put(renderer.p+0x7af28,32769U);a=snapshot("oversized-schedule");check(!a->scan_complete&&!a->stable&&!a->scanned,"bounded queue read");put(renderer.p+0x7af28,3U);
    put(spans.p+0x38,spans.p);a=snapshot("cyclic-spans");check(!a->trackers[0].complete&&a->trackers[0].count==16,"bounded cyclic span chain");put(spans.p+0x38,std::uintptr_t{});
    put(node.p,node.p);a=snapshot("cyclic-earlier");check(!a->earlier[0].complete&&a->earlier[0].visited==128&&a->earlier[0].count==16,"bounded earlier chain");put(node.p,std::uintptr_t{});
    put(backings.p+0x70,UINTPTR_MAX-8);a=snapshot("invalid-tracker");check(!a->trackers[0].readable&&!a->trackers[0].stable,"bad tracker diagnostic only");put(backings.p+0x70,trackers.p);
    unsigned changed{};
    struct Mutation {std::uintptr_t at;unsigned *count;};Mutation m{trackers.p+0x10,&changed};
    const tb::Reader changing{&m,[](void *user,std::uintptr_t at,std::span<std::byte> bytes){auto &v=*static_cast<Mutation *>(user);
        if(at==v.at&&bytes.size()==8){const auto value=std::uint64_t{9}+(*v.count)++;std::memcpy(bytes.data(),&value,8);return true;}
        return cd::reader.read(cd::reader.user,at,bytes);}};
    a=snapshot("changing-tracker",changing);check(!a->trackers[0].stable,"changed tracker not stable");
    changed=0;m.at=renderer.p+0x1f208;a=snapshot("changing-schedule",changing);check(!a->stable,"changed scheduler epoch not stable");
    put(renderer.p+0x7af28,70U);put(schedule.p+16*2+8,packets.p+0x90);put(schedule.p+16*69+8,packets.p+0x60);
    a=snapshot("bounded-window");check(a->scan_complete&&a->index==69&&a->earlier_count==32&&a->earlier[0].index==37,"bounded predecessor window");
    source.context=0;a=snapshot("missing-renderer");check(!a->readable&&!a->scan_complete,"missing renderer diagnostic only");
    check(cd::healthy(),"provenance reads never change renderer fault");
}

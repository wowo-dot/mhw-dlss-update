#pragma once
#include <type_traits>
void depth_eligibility_fixture(){
    const bool destination_mode=mode=="depth-destination";
    constexpr unsigned attachment_mask=(1U<<3)|(1U<<4)|(1U<<5)|(1U<<19);
    Block view(0x700),context(0x100),container(0xb0),owners(0x300),backings(0x100),srvs(0x40),attachment(0x70);
    tb::Snapshot source;source.view=view.p;source.context=context.p;source.frame=11;source.epoch=12;
    const auto setup=[&]{
        for(unsigned i=0;i<3;++i){const auto o=owners.p+0x100*i,b=backings.p+0x30*i;
            std::memset(reinterpret_cast<void *>(o),0,0x100);put(o,std::uintptr_t{1});put(o+0x60,b);put(o+0x88,2560U+(1440U<<16));
            put(o+0x94,41U);put(b+0x20,std::uintptr_t{0x10000+0x1000*i});}
        put(view.p+0x628,container.p);put(container.p+0xa0,owners.p);put(owners.p+0x78,srvs.p);put(srvs.p+0x10,std::uintptr_t{0x4444});
        put(srvs.p+0x30,std::uintptr_t{0x5555});put(context.p+0x50,attachment.p);put(attachment.p+0x58,owners.p+0x100);
    };
    setup();tb::SceneDepth expected;check(tb::scene_depth(cd::reader,source,expected),"reference depth");
    const cd::DepthEligibilityTrace::Identity at{100,11,12,13,14,15,16,view.p,context.p,0,0};
    const auto run=[&](const char *name,unsigned expected_difference,bool expected_readable=true,std::uintptr_t failed=0){
        struct Reads {std::uintptr_t failed{};unsigned calls{};};Reads baseline{failed},observed{failed};
        const auto read=[](void *user,std::uintptr_t p,std::span<std::byte> bytes){auto &s=*static_cast<Reads *>(user);++s.calls;
            return p!=s.failed&&cd::read(nullptr,p,bytes);};
        tb::SceneDepth a,b;const bool readable=tb::scene_depth({&baseline,read},source,a);
        cd::DepthReadObservation reads{{&observed,read}};const bool traced=tb::scene_depth(reads.reader(),source,b);
        check(readable==traced&&a==b&&baseline.calls==observed.calls&&reads.calls==baseline.calls,"reader preserves reads and result");
        const bool equal=traced&&b==expected;check(traced==expected_readable,"readable classification");
        if(traced)check(cd::depth_difference(cd::depth_fields(expected),cd::depth_fields(b))==expected_difference,"exact changed fields");
        check(equal==(traced&&!expected_difference),"original acceptance unchanged");
        cd::DepthEligibilityTrace trace;trace.scene(at,expected,b,traced,equal,reads);
        cd::DepthDestinationTrace destination;const bool admitted=destination.check(at,expected,b,traced);
        check(admitted==(traced&&!(expected_difference&~attachment_mask)),"destination acceptance boundary");
        std::cout<<std::boolalpha<<"{\"case\":\""<<name<<"\",\"dispatch_reasons\":[0,0,0,"<<static_cast<unsigned>(!(destination_mode?admitted:equal))<<",0,0,0,0,0,0,0,0],\"frames\":0,\"depth_eligibility\":";
        trace.write(std::cout);if(destination_mode){std::cout<<",\"depth_destination\":";destination.write(std::cout);}std::cout<<"}\n";
    };
    run("unchanged",0);
    put(owners.p+0x78,srvs.p+0x20);run("srv-and-descriptor",(1U<<1)|(1U<<2));setup();
    put(srvs.p+0x10,std::uintptr_t{0x5555});run("descriptor-only",1U<<2);setup();
    put(attachment.p+0x58,owners.p+0x200);run("attachment-owner-resource",(1U<<4)|(1U<<5));setup();
    put(context.p+0x50,std::uintptr_t{});run("attachment-unavailable",(1U<<3)|(1U<<4)|(1U<<5)|(1U<<19));setup();
    put(backings.p+0x20,std::uintptr_t{0x8888});run("texture-resource",1U<<18);setup();
    put(owners.p+0x70,1U);run("texture-flags",1U<<8);setup();
    run("optional-srv-read-failed",(1U<<2)|(1U<<20),true,srvs.p+0x10);
    run("required-read-failed",0,false,container.p+0xa0);
    put(view.p+0x628,std::uintptr_t{});run("invalid-container",0,false);setup();
    if(destination_mode){
        put(context.p+0x50,std::uintptr_t{});put(backings.p+0x20,std::uintptr_t{0x8888});run("attachment-and-texture-change",attachment_mask|(1U<<18));setup();
        run("attachment-read-failed",attachment_mask,true,context.p+0x50);
    }
    // Every compared SceneDepth member has a distinct serialized field.
    tb::SceneDepth fields=expected;const auto original=cd::depth_fields(fields);
    const auto field=[&](auto &value,unsigned bit){const auto saved=value;value=static_cast<std::remove_reference_t<decltype(value)>>(value+1);
        check(cd::depth_difference(original,cd::depth_fields(fields))==(1U<<bit),"complete field mapping");
        check(fields!=expected,"producer full equality preserved");
        check(cd::depth_destination_equal(expected,fields,true)==((attachment_mask&(1U<<bit))!=0),"explicit destination field contract");value=saved;};
    field(fields.container,0);field(fields.srv,1);field(fields.cpu_descriptor,2);field(fields.attachment,3);field(fields.attachment_owner,4);field(fields.attachment_resource,5);
    field(fields.texture.id,6);field(fields.texture.kind,7);field(fields.texture.flags,8);field(fields.texture.width,9);field(fields.texture.height,10);
    field(fields.texture.format,11);field(fields.texture.gpu_format,12);field(fields.texture.object,13);field(fields.texture.type,14);field(fields.texture.primary,15);
    field(fields.texture.secondary,16);field(fields.texture.backing,17);field(fields.texture.resource,18);
    fields.attachment_readable=false;check(cd::depth_difference(original,cd::depth_fields(fields))==(1U<<19)&&cd::depth_destination_equal(expected,fields,true),"attachment readability field");fields=expected;
    fields.srv_readable=false;check(cd::depth_difference(original,cd::depth_fields(fields))==(1U<<20)&&!cd::depth_destination_equal(expected,fields,true),"srv readability field");
    check(!cd::depth_destination_equal(expected,expected,false),"unreadable destination never accepted");
    for(unsigned bit=0;bit<7;++bit){cd::DepthEligibilityTrace trace;cd::DepthDestinationTrace destination;cd::DepthReadObservation good{cd::reader};
        trace.scene(at,expected,expected,true,true,good);check(destination.check(at,expected,expected,true),"lease fixture destination");std::array<std::uint64_t,5> actual{at.list,at.generation,at.frame,1,0};
        if(bit<3)++actual[bit];else if(bit<5)actual[bit]=1-actual[bit];else if(bit==5)actual={};
        const bool valid=actual[0]==at.list&&actual[1]==at.generation&&actual[2]==at.frame&&actual[3]&&!actual[4];
        trace.lease(at,actual[0],actual[1],actual[2],actual[3]!=0,actual[4]!=0);
        std::cout<<"{\"case\":\"lease-"<<bit<<"\",\"dispatch_reasons\":[0,0,0,0,0,0,0,"<<static_cast<unsigned>(!valid)<<",0,0,0,0],\"frames\":0,\"depth_eligibility\":";
        trace.write(std::cout);if(destination_mode){std::cout<<",\"depth_destination\":";destination.write(std::cout);}std::cout<<"}\n";
    }
    cd::DepthEligibilityTrace bounded;cd::DepthDestinationTrace destination;cd::DepthReadObservation reads{cd::reader};reads.calls=1;
    for(unsigned i=0;i<1000;++i){auto now=at;now.tick+=2*i;now.frame+=i;now.sequence+=2*i;auto changed=expected;++changed.cpu_descriptor;
        bounded.scene(now,expected,changed,true,false,reads);check(!destination.check(now,expected,changed,true),"bounded descriptor rejection");
        bounded.scene(now,expected,expected,true,true,reads);check(destination.check(now,expected,expected,true),"bounded unchanged destination");bounded.lease(now,0,0,0,false,false);
        if(destination_mode){auto attachment_change=expected;++attachment_change.attachment;
            // Distinct timestamp/sequence for a distinct diagnostic observation.
            ++now.tick;++now.sequence;
            bounded.scene(now,expected,attachment_change,true,false,reads);check(destination.check(now,expected,attachment_change,true),"bounded attachment admission");}}
    std::ostringstream out;out<<std::boolalpha;bounded.write(out);check(out.str().size()<16384,"bounded first/latest trace");
    std::cout<<"{\"case\":\"bounded\",\"dispatch_reasons\":[0,0,0,1000,0,0,0,1000,0,0,0,0],\"frames\":0,\"depth_eligibility\":"<<out.str();
    if(destination_mode){std::cout<<",\"depth_destination\":";destination.write(std::cout);}std::cout<<"}\n";
}

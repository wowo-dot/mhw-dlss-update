#include "../src/aa_control/owned_pass.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
namespace aa=mhw::aa_control;
constexpr std::uintptr_t image=0x140000000,descriptor=0x900000,unit=0x900100,vtable=0x900200;
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
aa::PostQueue fixture(bool haze=false){
    aa::PostQueue q;q.manager=1;q.context=2;q.view=3;q.renderer=4;q.frame=5;q.epoch=6;q.bank=1;
    q.count=4;q.enabled=q.collecting=1;q.rectangle={0,0,2560,1440};q.scratch={20,30};q.attachments={10,40};
    q.nodes[0]={100,1000,image+0x3538400,10,20,15,0,{0,0,0}};
    q.nodes[1]={200,2000,image+0x3537a70,20,30,15,100,{0,0,0}};
    q.nodes[2]={300,3000,image+0x3536c98,0,30,15,1024,{0,1,0}};
    q.nodes[3]={400,0,0,30,40,15,134217713,{1,0,0}};
    if(haze){q.count=5;q.nodes[4]={500,5000,image+0x35372b0,0,0,15,20000,{0,0,0}};}
    return q;
}
// Small independent translation of the reviewed native owner-assignment rules,
// including the endpoint cases: inserting an ordinary no-op filter changes the
// chain, while an interior +21 marker preserves it. This is CPU layout evidence.
aa::PostQueue assign(aa::PostQueue q){
    std::sort(q.nodes.begin(),q.nodes.begin()+q.count,[](const auto &a,const auto &b){return a.order<b.order;});
    unsigned bank{};
    for(unsigned i=0;i<q.count;++i){auto &n=q.nodes[i];
        if(n.flags[1]){
            n.input=0;
            n.output=(i==q.count-1)?q.attachments[1]:q.scratch[bank&1];
        }else{
            const bool first_color=n.phase==15&&n.order==0;
            n.input=(i==0&&first_color)?q.attachments[0]:q.scratch[bank&1];
            if(i==q.count-1&&!n.flags[2])n.output=q.attachments[1];
            else {if(!first_color)++bank;n.output=q.scratch[bank&1];}
        }
    }
    q.collecting=0;q.packet_count+=31;return q;
}
int main(){
    unsigned permutations{};
    for(const bool haze:{false,true}){auto q=fixture(haze);
    std::sort(q.nodes.begin(),q.nodes.begin()+q.count,[](const auto &a,const auto &b){return a.order<b.order;});
    do {
        aa::OwnedPlan p;check(aa::plan_owned_pass(q,image,descriptor,unit,vtable,p),"all native enqueue orders admitted");
        const auto native=assign(q);
        auto with=q;with.nodes[q.count]={descriptor,unit,vtable,0,0,15,2,{0,1,0}};++with.count;
        auto result=assign(with);
        check(aa::owned_callback_matches(p,result)&&aa::owned_return_matches(p,result),"in-place callback preserves native endpoints");
        for(unsigned i=0;i<native.count;++i){const auto &n=native.nodes[i];
            const auto found=std::find_if(result.nodes.begin(),result.nodes.begin()+result.count,[&](const auto &v){return v.descriptor==n.descriptor;});
            check(found!=result.nodes.begin()+result.count&&*found==n,"mod insertion equals unmodified native color graph");}
        if(haze){check(result.nodes[4].order==20000&&result.nodes[4].input==30&&result.nodes[4].output==20&&
                       result.nodes[5].input==20&&result.nodes[5].output==40,"literal haze and final-copy endpoints");}
        auto bad=result;bad.nodes[1].flags[1]=0;bad=assign(bad);
        check(!aa::owned_callback_matches(p,bad),"ordinary no-op filter is not neutral");
        for(unsigned i=0;i<result.count;++i){bad=result;bad.nodes[i].output++;check(!aa::owned_callback_matches(p,bad),"every output checked");
            bad=result;bad.nodes[i].input++;check(!aa::owned_callback_matches(p,bad),"every assigned input checked");}
        bad=result;bad.frame++;check(!aa::owned_callback_matches(p,bad),"same frame required");
        bad=result;bad.nodes[1].unit++;check(!aa::owned_callback_matches(p,bad),"owned identity required");
        bad=result;std::swap(bad.nodes[0],bad.nodes[2]);check(!aa::owned_callback_matches(p,bad),"native sort order required");
        auto restored=q;restored.collecting=0;restored.packet_count=result.packet_count;
        for(unsigned i=0;i<restored.count;++i){const auto id=restored.nodes[i].descriptor;
            restored.nodes[i]=*std::find_if(native.nodes.begin(),native.nodes.begin()+native.count,[&](const auto &v){return v.descriptor==id;});}
        check(aa::owned_restored_matches(p,restored),"original queue ordering restored");
        restored.nodes[0].output++;check(!aa::owned_restored_matches(p,restored),"native endpoint corruption rejected");
        ++permutations;
    }while(std::next_permutation(q.nodes.begin(),q.nodes.begin()+q.count,[](const auto &a,const auto &b){return a.order<b.order;}));}
    check(permutations==144,"all base and haze queue permutations exercised");
    auto q=fixture();
    aa::OwnedPlan p;auto reject=[&](aa::PostQueue bad){check(!aa::plan_owned_pass(bad,image,descriptor,unit,vtable,p)&&!p.descriptor,"reject before mutation");};
    for(unsigned i=0;i<4;++i){auto bad=q;bad.nodes[i].phase++;reject(bad);bad=q;bad.nodes[i].vtable++;reject(bad);
        bad=q;bad.nodes[i].flags[2]=1;reject(bad);bad=q;bad.nodes[i].order=2;reject(bad);}
    auto bad=q;bad.taa=1;reject(bad);bad=q;bad.fxaa=1;reject(bad);bad=q;bad.count=16;reject(bad);
    bad=q;bad.scratch[1]=bad.scratch[0];reject(bad);bad=q;bad.collecting=0;reject(bad);
    bad=q;bad.nodes[1].descriptor=bad.nodes[0].descriptor;reject(bad);
    bad=fixture(true);bad.nodes[4].vtable++;reject(bad);
    bad=fixture(true);bad.nodes[4].flags[1]=1;reject(bad);
    bad=fixture(true);bad.nodes[4].order=2;reject(bad);
    bad=fixture(true);bad.nodes[4].unit=0;reject(bad);
    bad=fixture(true);bad.nodes[4]=bad.nodes[2];reject(bad);
    for(const bool haze:{false,true}){auto stale=fixture(haze);
        for(auto &n:stale.nodes){n.input=0x12345678;n.output=0x87654321;}
        check(aa::plan_owned_pass(stale,image,descriptor,unit,vtable,p),"collection-time endpoints are overwritten, never dereferenced");
        auto with=stale;with.nodes[with.count++]={descriptor,unit,vtable,0,0,15,2,{0,1,0}};
        check(aa::owned_callback_matches(p,assign(with)),"stale collection endpoints converge to validated native graph");}
    auto removed=fixture();removed.nodes[3].input=20;
    check(aa::plan_owned_pass(removed,image,descriptor,unit,vtable,p)&&p.assigned.nodes[3].input==30,"haze removal restores base copy source");
    std::cout<<"Owned in-place pass: 144 permutations, native graph equivalence, haze appearance/removal and corruption rejection passed\n";
}

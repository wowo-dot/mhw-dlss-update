#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <algorithm>
#include <ostream>
namespace mhw::continuous_dlaa {
// Observe the existing reader without adding reads or changing short-circuiting.
// Addresses are diagnostic identities only; no memory is dereferenced by logging.
struct DepthReadObservation {
    temporal_bindings::Reader source;
    std::uint64_t calls{},failures{},first_call{},first_address{},first_bytes{};
    static bool read(void *user,std::uintptr_t at,std::span<std::byte> bytes){
        auto &s=*static_cast<DepthReadObservation *>(user);++s.calls;
        const bool ok=s.source.read&&s.source.read(s.source.user,at,bytes);
        if(!ok){if(!s.failures){s.first_call=s.calls;s.first_address=at;s.first_bytes=bytes.size();}++s.failures;}return ok;
    }
    temporal_bindings::Reader reader() noexcept {return {this,read};}
};
inline std::array<std::uint64_t,21> depth_fields(const temporal_bindings::SceneDepth &d) noexcept {
    const auto &t=d.texture;
    return {d.container,d.srv,d.cpu_descriptor,d.attachment,d.attachment_owner,d.attachment_resource,
        t.id,t.kind,t.flags,t.width,t.height,t.format,t.gpu_format,t.object,t.type,t.primary,t.secondary,t.backing,t.resource,
        d.attachment_readable,d.srv_readable};
}
template<std::size_t N>unsigned depth_difference(const std::array<std::uint64_t,N> &a,const std::array<std::uint64_t,N> &b) noexcept {
    static_assert(N<=31);unsigned mask{};for(unsigned i=0;i<N;++i)if(a[i]!=b[i])mask|=1U<<i;return mask;
}
// Protected by the existing runtime mutex. First eight + latest failures per
// stage; no per-frame allocation, file I/O, new hooks, or resource ownership.
class DepthEligibilityTrace {
public:
    struct Identity {std::uint64_t tick{},frame{},epoch{},sequence{},packet{},list{},generation{},view{},context{},bank{},async{};};
private:
    struct Scene {std::uint64_t ordinal{};Identity at;bool readable{},equal{};unsigned difference{};
        std::array<std::uint64_t,21> expected{},actual{};std::array<std::uint64_t,5> reads{};};
    struct Lease {std::uint64_t ordinal{};Identity at;unsigned difference{};std::array<std::uint64_t,5> actual{};};
    template<class T>struct Samples {
        std::uint64_t total{};std::array<T,8> first{};T latest{};
        void add(T value) noexcept {value.ordinal=++total;if(total<=first.size())first[static_cast<std::size_t>(total-1)]=value;else latest=value;}
        template<class F>void write(std::ostream &out,F emit)const {
            out<<'[';const auto count=static_cast<unsigned>((std::min)(total,std::uint64_t{8}));
            for(unsigned i=0;i<count;++i){if(i)out<<',';emit(first[i]);}if(total>8){out<<',';emit(latest);}out<<']';
        }
    };
    Samples<Scene> scene_;Samples<Lease> lease_;
    std::uint64_t scene_checks_{},scene_passes_{},unreadable_{},mismatches_{},lease_checks_{},lease_passes_{};
    std::array<std::uint64_t,21> scene_differences_{};
    std::array<std::uint64_t,5> lease_differences_{};
    template<std::size_t N>static void array(std::ostream &out,const std::array<std::uint64_t,N> &v){out<<'[';for(unsigned i=0;i<N;++i)out<<(i?",":"")<<v[i];out<<']';}
    static void identity(std::ostream &out,const Identity &a){
        array(out,std::array<std::uint64_t,11>{a.tick,a.frame,a.epoch,a.sequence,a.packet,a.list,a.generation,a.view,a.context,a.bank,a.async});
    }
public:
    void scene(const Identity &at,const temporal_bindings::SceneDepth &expected,const temporal_bindings::SceneDepth &actual,
               bool readable,bool equal,const DepthReadObservation &reads) noexcept {
        ++scene_checks_;if(equal){++scene_passes_;return;}
        Scene sample;sample.at=at;sample.readable=readable;sample.equal=equal;sample.expected=depth_fields(expected);sample.actual=depth_fields(actual);
        sample.reads={reads.calls,reads.failures,reads.first_call,reads.first_address,reads.first_bytes};
        if(!readable)++unreadable_;else {++mismatches_;sample.difference=depth_difference(sample.expected,sample.actual);
            for(unsigned i=0;i<scene_differences_.size();++i)if(sample.difference&(1U<<i))++scene_differences_[i];}
        scene_.add(sample);
    }
    void lease(const Identity &at,std::uintptr_t list,std::uint64_t generation,std::uint64_t frame,bool depth,bool evaluated) noexcept {
        ++lease_checks_;const std::array<std::uint64_t,5> actual{list,generation,frame,depth,evaluated};
        const unsigned difference=depth_difference(std::array<std::uint64_t,5>{at.list,at.generation,at.frame,1,0},actual);
        if(!difference){++lease_passes_;return;}
        for(unsigned i=0;i<lease_differences_.size();++i)if(difference&(1U<<i))++lease_differences_[i];
        lease_.add({0,at,difference,actual});
    }
    void write(std::ostream &out)const {
        out<<"{\"schema\":1,\"policy\":\"depth-eligibility-observation-v1\",\"diagnostic_only\":true,\"scene\":{\"checks\":"<<scene_checks_
           <<",\"passes\":"<<scene_passes_<<",\"unreadable\":"<<unreadable_<<",\"mismatches\":"<<mismatches_<<",\"rejected\":"<<scene_.total<<",\"differences\":";
        array(out,scene_differences_);out<<",\"samples\":";scene_.write(out,[&](const Scene &s){out<<"{\"ordinal\":"<<s.ordinal<<",\"at\":";identity(out,s.at);
            out<<",\"readable\":"<<s.readable<<",\"equal\":"<<s.equal<<",\"difference\":"<<s.difference<<",\"expected\":";array(out,s.expected);
            out<<",\"actual\":";array(out,s.actual);out<<",\"reads\":";array(out,s.reads);out<<'}';});
        out<<"},\"lease\":{\"checks\":"<<lease_checks_<<",\"passes\":"<<lease_passes_<<",\"rejected\":"<<lease_.total<<",\"differences\":";
        array(out,lease_differences_);out<<",\"samples\":";lease_.write(out,[&](const Lease &s){out<<"{\"ordinal\":"<<s.ordinal<<",\"at\":";identity(out,s.at);
            out<<",\"difference\":"<<s.difference<<",\"actual\":";array(out,s.actual);out<<'}';});out<<"}}";
    }
};
}

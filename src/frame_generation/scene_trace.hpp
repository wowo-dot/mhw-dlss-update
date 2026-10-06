#pragma once
#include <array>
#include <cstdint>
#include <ostream>
#include <span>

namespace mhw::frame_generation {
// CPU observations only. Caller serializes every operation. Pointer values
// are labels, never references, and submission receipts are not GPU fences.
class SceneTrace final {
public:
    static constexpr unsigned capacity=4,event_capacity=512,resource_capacity=16,rejection_reasons=16,samples_per_reason=2,none=UINT32_MAX;
    struct Identity {
        std::uint64_t frame{},epoch{},context{},view{},packet{},source{},target{};
        unsigned width{},height{},format{},bank{};
    };
    struct Event {
        // kind: 0 draw, 1 indexed draw, 2 transition, 3 alias/UAV barrier, 4 copy.
        unsigned kind{},thread{},stage{none},flags{},subresource{},before{},after{};
        std::uint64_t ordinal{},qpc{},list{},generation{},caller{},packet{},resource{},rtv{},root{},pipeline{},shader{},native_frame{},native_epoch{};
        std::array<std::uint64_t,5> args{};
        // Copy: src/dst, types, subresources, dst XYZ, has-box, L/T/F/R/B/Bk.
        // Only SUBRESOURCE_INDEX texture copies are followed, never footprints.
        std::array<std::uint64_t,16> copy{};
        // State/list/packet/attachment/active/flags/view/backend/native RTV/root/
        // PSO, plus successful read bits. Unread fields are zero, never identities.
        std::array<std::uint64_t,11> native_cache{};
        unsigned native_cache_read_mask{};
        std::array<std::uint64_t,19> raster_observation{};
        bool api_state_matched{},pipeline_read{},returned{},replaced{},reset{};
        unsigned rotation{},blend{},depth{};
        std::uint64_t batch{},queue{},submit_qpc{};unsigned batch_index{},batch_count{};
    };
    struct Frame {
        unsigned request{},count{};Identity identity;
        std::uint64_t events{},unmatched{},unreadable{},outside_scope{},overflow{},duplicate_submissions{},premature_resets{};
        std::uint64_t present_qpc{},swap{},present_frame{},present_epoch{};unsigned reflex_id{},present_thread{},choice{};
        bool presented{};
        std::array<Event,event_capacity> observations{};
        unsigned resource_count{},sample_count{};
        std::uint64_t resource_overflow{};
        std::array<std::uint64_t,resource_capacity> resources{};
        std::array<std::uint64_t,rejection_reasons> rejected{};
        // Rejected entry snapshots are incomplete and never gate Present.
        std::array<Event,rejection_reasons*samples_per_reason> samples{};
        std::array<unsigned,rejection_reasons*samples_per_reason> reasons{};
    };
private:
    std::array<Frame,capacity> frames_{};
    unsigned pending_{},requested_{},count_{};
    std::uint64_t batches_{};
public:
    bool request(unsigned id) noexcept {
        if(pending_||requested_>=capacity||id!=requested_+1)return false;
        if(count_&&!frames_[count_-1].presented)return false;
        pending_=id;++requested_;return true;
    }
    bool wants_frame() const noexcept {return pending_!=0;}
    bool begin(Identity id) noexcept {
        if(!pending_||count_>=capacity||!id.frame||!id.context||!id.view||!id.packet||
           !id.source||!id.target||id.source==id.target||!id.width||!id.height||id.bank>=2)return false;
        auto &f=frames_[count_++];f.request=pending_;f.identity=id;pending_=0;
        f.resources[f.resource_count++]=id.target;return true;
    }
    Frame* active(std::uint64_t frame,std::uint64_t epoch) noexcept {
        if(!count_)return nullptr;auto &f=frames_[count_-1];
        return !f.presented&&f.identity.frame==frame&&f.identity.epoch==epoch?&f:nullptr;
    }
    Frame* recording() noexcept {
        return count_&&!frames_[count_-1].presented?&frames_[count_-1]:nullptr;
    }
    bool tracked(std::uint64_t resource) const noexcept {
        if(!count_||frames_[count_-1].presented||!resource)return false;
        const auto &f=frames_[count_-1];
        for(unsigned i=0;i<f.resource_count;++i)if(f.resources[i]==resource)return true;
        return false;
    }
    void reject(unsigned reason,Event sample) noexcept {
        auto *f=recording();if(!f||reason>=rejection_reasons)return;
        const auto n=f->rejected[reason]++;
        if(n>=samples_per_reason)return;
        const auto slot=f->sample_count++;f->samples[slot]=sample;f->reasons[slot]=reason;
    }
    unsigned event(std::uint64_t frame,std::uint64_t epoch,Event event) noexcept {
        auto *f=active(frame,epoch);if(!f)return none;
        ++f->events;
        if(f->count==event_capacity){++f->overflow;return none;}
        event.ordinal=f->events;f->observations[f->count]=event;return f->count++;
    }
    void returned(std::uint64_t frame,std::uint64_t epoch,unsigned index,bool ok,bool replaced) noexcept {
        auto *f=active(frame,epoch);if(!f||index>=f->count)return;
        auto &e=f->observations[index];e.returned=ok;e.replaced=replaced;
        // This is a CPU-observed copy path, not a GPU dependency/lifetime proof.
        if(e.kind!=4||!ok||replaced||!tracked(e.copy[0])||tracked(e.copy[1]))return;
        if(f->resource_count==resource_capacity){++f->resource_overflow;return;}
        f->resources[f->resource_count++]=e.copy[1];
    }
    void choice(std::uint64_t frame,std::uint64_t epoch,unsigned choice) noexcept {
        if(auto *f=active(frame,epoch))f->choice=choice;
    }
    void submit(std::uint64_t queue,std::uint64_t qpc,std::span<const std::array<std::uint64_t,2>> lists) noexcept {
        const auto batch=++batches_;
        for(unsigned i=0;i<count_;++i){auto &f=frames_[i];if(f.presented)continue;
            for(unsigned j=0;j<lists.size();++j)for(unsigned k=0;k<f.count;++k){auto &e=f.observations[k];
                if(e.list!=lists[j][0]||e.generation!=lists[j][1])continue;
                if(e.batch){++f.duplicate_submissions;continue;}
                e.batch=batch;e.queue=queue;e.submit_qpc=qpc;e.batch_index=j;e.batch_count=static_cast<unsigned>(lists.size());
            }
            for(unsigned j=0;j<lists.size();++j)for(unsigned k=0;k<f.sample_count;++k){auto &e=f.samples[k];
                if(e.list!=lists[j][0]||!e.generation||e.generation!=lists[j][1]||e.batch)continue;
                e.batch=batch;e.queue=queue;e.submit_qpc=qpc;e.batch_index=j;e.batch_count=static_cast<unsigned>(lists.size());
            }
        }
    }
    void reset(std::uint64_t list,std::uint64_t generation) noexcept {
        for(unsigned i=0;i<count_;++i){auto &f=frames_[i];
            for(unsigned j=0;j<f.count;++j){auto &e=f.observations[j];
                if(e.list!=list||e.generation!=generation)continue;
                if(!e.batch)++f.premature_resets;e.reset=true;
            }
            for(unsigned j=0;j<f.sample_count;++j){auto &e=f.samples[j];
                if(e.list==list&&e.generation==generation)e.reset=true;
            }
        }
    }
    void present(std::uint64_t frame,std::uint64_t epoch,unsigned reflex_id,unsigned thread,
                 std::uint64_t swap,std::uint64_t qpc) noexcept {
        auto *f=recording();if(!f||!frame||!reflex_id||!thread||!swap||!qpc||!f->choice)return;
        unsigned final{};
        for(unsigned i=0;i<f->count;++i){const auto &e=f->observations[i];
            if(!e.returned||!e.batch||e.qpc>e.submit_qpc||e.submit_qpc>qpc)return;
            if(e.stage==2){if(e.kind||e.packet!=f->identity.packet||!e.replaced||!e.api_state_matched||!e.pipeline_read)return;++final;}
        }
        // Native preparation and recording overlap. Close on the qualified
        // Present after this exact final draw has returned and been submitted,
        // rather than equating the producer's counter with Present's counter.
        if(final!=1||f->overflow||f->duplicate_submissions||f->premature_resets)return;
        f->presented=true;f->reflex_id=reflex_id;f->present_frame=frame;f->present_epoch=epoch;
        f->present_thread=thread;f->swap=swap;f->present_qpc=qpc;
    }
    unsigned size() const noexcept {return count_;}
    const Frame& frame(unsigned index) const noexcept {return frames_[index];}
    void write(std::ostream &o) const {
        o<<"{\"schema\":4,\"policy\":\"submitted-scene-raster-scope-map-v4\",\"diagnostic_only\":true,\"gpu_work_added\":false,"
            "\"pointers_owned\":false,\"pixel_contents_verified\":false,\"GPU_completion_verified\":false,"
            "\"capacity\":"<<capacity<<",\"event_capacity\":"<<event_capacity<<",\"requested\":"<<requested_
            <<",\"pending\":"<<pending_<<",\"resource_capacity\":"<<resource_capacity
            <<",\"rejection_reasons\":"<<rejection_reasons<<",\"samples_per_reason\":"<<samples_per_reason<<",\"captures\":[";
        for(unsigned i=0;i<count_;++i){if(i)o<<',';const auto &f=frames_[i];const auto &v=f.identity;
            o<<"{\"request\":"<<f.request<<",\"identity\":["<<v.frame<<','<<v.epoch<<','<<v.context<<','<<v.view<<','
                <<v.packet<<','<<v.source<<','<<v.target<<','<<v.width<<','<<v.height<<','<<v.format<<','<<v.bank<<']'
                <<",\"events\":"<<f.events<<",\"unmatched\":"<<f.unmatched<<",\"unreadable\":"<<f.unreadable
                <<",\"outside_scope\":"<<f.outside_scope<<",\"overflow\":"<<f.overflow
                <<",\"duplicate_submissions\":"<<f.duplicate_submissions<<",\"premature_resets\":"<<f.premature_resets
                <<",\"choice\":"<<f.choice<<",\"presented\":"<<f.presented<<",\"reflex_id\":"<<f.reflex_id
                <<",\"present_thread\":"<<f.present_thread<<",\"swap\":"<<f.swap<<",\"present_qpc\":"<<f.present_qpc
                <<",\"present_frame\":"<<f.present_frame<<",\"present_epoch\":"<<f.present_epoch
                <<",\"resource_overflow\":"<<f.resource_overflow<<",\"followed_resources\":[";
            for(unsigned j=0;j<f.resource_count;++j){if(j)o<<',';o<<f.resources[j];}
            o<<"],\"rejection_counts\":[";
            for(unsigned j=0;j<rejection_reasons;++j){if(j)o<<',';o<<f.rejected[j];}
            o<<"],\"rejection_samples\":[";
            for(unsigned j=0;j<f.sample_count;++j){if(j)o<<',';const auto &e=f.samples[j];
                o<<"{\"reason\":"<<f.reasons[j]<<",\"kind\":"<<e.kind<<",\"qpc\":"<<e.qpc<<",\"thread\":"<<e.thread
                    <<",\"caller\":"<<e.caller<<",\"list\":"<<e.list<<",\"generation\":"<<e.generation
                    <<",\"native_frame\":"<<e.native_frame<<",\"native_epoch\":"<<e.native_epoch
                    <<",\"packet\":"<<e.packet<<",\"resource\":"<<e.resource<<",\"rtv\":"<<e.rtv
                    <<",\"root\":"<<e.root<<",\"pipeline\":"<<e.pipeline<<",\"batch\":"<<e.batch<<",\"queue\":"<<e.queue
                    <<",\"submit_qpc\":"<<e.submit_qpc<<",\"batch_index\":"<<e.batch_index<<",\"batch_count\":"<<e.batch_count
                    <<",\"reset\":"<<e.reset<<",\"native_cache\":[";
                for(unsigned n=0;n<e.native_cache.size();++n){if(n)o<<',';o<<e.native_cache[n];}
                o<<"],\"native_cache_read_mask\":"<<e.native_cache_read_mask<<",\"raster_observation\":[";
                for(unsigned n=0;n<e.raster_observation.size();++n){if(n)o<<',';o<<e.raster_observation[n];}
                o<<"]}";
            }
            o<<"],\"observations\":[";
            for(unsigned j=0;j<f.count;++j){if(j)o<<',';const auto &e=f.observations[j];
                o<<"{\"kind\":"<<e.kind<<",\"ordinal\":"<<e.ordinal<<",\"qpc\":"<<e.qpc<<",\"thread\":"<<e.thread
                    <<",\"stage\":"<<e.stage<<",\"list\":"<<e.list<<",\"generation\":"<<e.generation<<",\"caller\":"<<e.caller
                    <<",\"packet\":"<<e.packet<<",\"resource\":"<<e.resource<<",\"native_frame\":"<<e.native_frame
                    <<",\"native_epoch\":"<<e.native_epoch<<",\"rtv\":"<<e.rtv<<",\"root\":"<<e.root
                    <<",\"pipeline\":"<<e.pipeline<<",\"shader\":"<<e.shader<<",\"args\":[";
                for(unsigned n=0;n<e.args.size();++n){if(n)o<<',';o<<e.args[n];}
                o<<"],\"copy\":[";
                for(unsigned n=0;n<e.copy.size();++n){if(n)o<<',';o<<e.copy[n];}
                o<<"],\"native_cache\":[";
                for(unsigned n=0;n<e.native_cache.size();++n){if(n)o<<',';o<<e.native_cache[n];}
                o<<"],\"native_cache_read_mask\":"<<e.native_cache_read_mask<<",\"raster_observation\":[";
                for(unsigned n=0;n<e.raster_observation.size();++n){if(n)o<<',';o<<e.raster_observation[n];}
                o<<"],\"api_state_matched\":"<<e.api_state_matched<<",\"pipeline_read\":"<<e.pipeline_read
                    <<",\"returned\":"<<e.returned<<",\"replaced\":"<<e.replaced<<",\"rotation\":"<<e.rotation
                    <<",\"blend\":"<<e.blend<<",\"depth\":"<<e.depth<<",\"flags\":"<<e.flags<<",\"subresource\":"<<e.subresource
                    <<",\"before\":"<<e.before<<",\"after\":"<<e.after<<",\"batch\":"<<e.batch<<",\"queue\":"<<e.queue
                    <<",\"submit_qpc\":"<<e.submit_qpc<<",\"batch_index\":"<<e.batch_index<<",\"batch_count\":"<<e.batch_count
                    <<",\"reset\":"<<e.reset<<'}';
            }o<<"]}";
        }o<<"]}";
    }
};
}

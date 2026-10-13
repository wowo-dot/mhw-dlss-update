// full_post (prototype; full_post.hpp, docs/research/full-size-post-chain.md).
// Included after the final-quad capture. Every state change happens on the
// render thread that records the camera's scene and post chain: the state is
// thread-local, so any other thread sees it unarmed. Only the armed frame
// number is shared with the plan builder (the same thread, after the queue).
using FullViewportFn=void(*)(std::uintptr_t context,const int *rect);
using FullCopyFn=void(*)(std::uintptr_t context,std::uintptr_t destination,int destination_subresource,const unsigned *xy,
                         std::uintptr_t source,int source_subresource,const unsigned *rect,const unsigned *optional);
// The game's setViewport (0x23d0330) and copy helper (0x23c9250), set when
// the hooks are installed. Fixtures may supply their own.
FullViewportFn full_viewport{};FullCopyFn full_copy{};
struct FullPostState {
 std::uint64_t frame{};std::uintptr_t context{};
 bool armed{},in_scheduler{},switched{},tone_map_full{};
 std::array<int,4> render_rect{},full_rect{};
};
thread_local FullPostState full_state;
// The frame of this thread's last TAA callback: a frame is armed only when the
// previous one ran TAA, so the switch will happen (the final copy is recorded
// before the post chain).
thread_local std::uint64_t full_taa_frame{UINT64_MAX};
std::atomic<std::uint64_t> full_frame{UINT64_MAX};  // the last armed frame (plan builder)
struct FullPostCounters {
 std::atomic<std::uint64_t> final_copies{},schedulers{},switches{},tone_map_switches{},projections{},restores{},
  not_switched{},rect_mismatch{},stale{},declined{},no_taa{},identity_plans{};
 // The inputs' upscale (v0.25.2): slots recorded, refused (by reason),
 // planned, executed with the upscale, executed without it (by reason).
 std::atomic<std::uint64_t> slots{},slots_planned{},upscales{},slot_unverified{},slot_stale{};
 std::array<std::atomic<std::uint64_t>,8> slot_refusals{},slot_skips{};
} full_counts;
// The render-size inputs of the post chain, upscaled at the slot
// (full_post_inputs.hpp). Prepared with the route at the output size.
FullPostInputs full_inputs;std::string full_inputs_failure;
// The upscale slot: an engine copy packet the post scheduler records right
// after TAA's callback, at key (15, 3) -- after TAA's (15, 2) and before every
// later post effect -- from GBuffer4 into GBuffer5. The game's barrier planner
// gives that packet GBuffer5 in COPY_DEST and GBuffer4 in COPY_SOURCE; its
// copy never runs: the copy hook records the upscale there instead
// (full_post_slot.inl), or nothing. The depth is not part of the packet: a
// new state span on it after TAA could have its transition hoisted before the
// TAA consumer, whose depth admission accepts only reads there.
struct FullSlot {
 std::uint64_t frame{};std::uintptr_t context{},packet{};
 tb::Owner depth,gbuffer4,gbuffer5;
 bool valid{},executed{};
};
std::array<FullSlot,8> full_slots;  // frame % 8, under the route lock
constexpr unsigned full_slot_key=(15U<<27)|3U;
// Records the slot on the post scheduler's thread, inside TAA's key. 0: the
// slot was recorded and verified; otherwise the refusal (nothing recorded),
// or 7 (recorded, not verified: registered so that its copy never runs).
unsigned full_slot_record(std::uintptr_t context,std::uint64_t frame) noexcept {
 if(!full_inputs.ready()||!full_copy)return 1;
 std::uintptr_t view{},container{},depth{},gbuffer{},gb4{},gb5{},filters{},array{};unsigned key{},sub{},count{},filter_count{};
 if(!field(context,0x1d0,view)||!field(view,0x628,container)||!field(container,0xa0,depth)||!field(view,0x620,gbuffer)||
    !field(gbuffer,0x78,gb4)||!field(gbuffer,0x80,gb5))return 2;
 FullSlot slot;slot.frame=frame;slot.context=context;
 if(!tb::read_owner(reader,depth,slot.depth)||!tb::read_owner(reader,gb4,slot.gbuffer4)||!tb::read_owner(reader,gb5,slot.gbuffer5))return 3;
 // Output-size textures, and not the executor's kind-6 (whole-resource) path.
 for(const auto *o:{&slot.depth,&slot.gbuffer4,&slot.gbuffer5})if(o->width!=width||o->height!=height||o->kind==6)return 3;
 if(slot.depth.resource==slot.gbuffer4.resource||slot.depth.resource==slot.gbuffer5.resource||slot.gbuffer4.resource==slot.gbuffer5.resource)return 3;
 // TAA's key, with no added order (cDraw +0x154 stage | order << 5; the
 // copy helper adds +0x15c >> 5).
 if(!field(context,0x154,key)||(key&31)!=15||(key>>5)!=2||!field(context,0x15c,sub)||(sub>>5))return 4;
 // No registered post filter shares the slot's order.
 if(!field(cfg.image,0x51cc238,filters)||!field(filters,0x3c,filter_count)||filter_count>16)return 5;
 for(unsigned i=0;i<filter_count;++i){std::uintptr_t e{};unsigned stage{},order{};
  if(!field(filters,0x40+8*i,e)||!field(e,0,stage)||!field(e,4,order))return 5;
  if(stage==15&&order==3)return 5;}
 if(!field(context,0x2b80,count)||!field(context,0x2b78,array)||!array||count>=65535)return 6;
 // The game's own order change (as the scheduler makes for each filter), for
 // this one packet; TAA's key is restored right after.
 auto *order=reinterpret_cast<volatile unsigned *>(context+0x154);
 const unsigned xy[2]{},rect[4]{0,0,1,1};
 *order=(key&31)|(3U<<5);full_copy(context,slot.gbuffer5.object,0,xy,slot.gbuffer4.object,0,rect,nullptr);*order=key;
 // The command array entry the helper appended, found by its owners: it is
 // registered even when it does not verify, so that its copy never runs. Not
 // found, the route stops (fault 15).
 struct Command {unsigned key{},padding{};std::uintptr_t packet{};} c;static_assert(sizeof(Command)==16);
 unsigned after{},raw{};std::uintptr_t destination{},source{};
 if(!pb::copy_memory(array+16ULL*count,&c,sizeof(c))||!c.packet||!field(c.packet,0x10,destination)||destination!=slot.gbuffer5.object||
    !field(c.packet,0x18,source)||source!=slot.gbuffer4.object){fail(15);return 6;}
 slot.packet=c.packet;
 slot.valid=c.key==full_slot_key&&field(context,0x2b80,after)&&after==count+1&&field(c.packet,0,raw)&&(raw&31)==13;
 if(!slot.valid)++full_counts.slot_unverified;
 {Lock lock;auto &q=full_slots[frame%full_slots.size()];if(q.packet&&!q.executed)++full_counts.slot_stale;q=slot;}
 ++full_counts.slots;return slot.valid?0:7;
}
// The depth's state at the TAA consumer, from its admission (published by the
// continuous runtime with the depth copy for DLSS).
void full_depth(const tb::Snapshot &s,unsigned state,std::uintptr_t resource) noexcept {
 Lock lock;auto &p=plans[s.frame%plans.size()];
 if(p.prepared&&p.stamp==s&&p.full_slot){p.full_depth_state=state;p.full_depth_resource=resource;p.full_depth_known=true;}
}
// The final quad of a scaled frame becomes the game's DLAA copy (the same
// owners as the quad: tBaseMap's texture into the view's attachment, the full
// rectangle), which arms this thread's post chain for the frame. slot: the
// final quad's capture (final_before), entered.
bool full_final(const pb::Frame &f,unsigned slot){
 if(!full_post_enabled.load()||!full_copy||!full_viewport||!full_inputs.ready()||slot==UINT_MAX||fault.load()||!accepting.load()||(render_w==width&&render_h==height))return false;
 FinalQuad q;{Lock lock;q=final_quads[slot];if(!q.entered||q.stamp.context!=f.gpr[2]){++full_counts.declined;return false;}
  if(full_taa_frame+1!=q.stamp.frame){++full_counts.no_taa;return false;}
  final_quads[slot].full_post=true;}
 const unsigned xy[2]{},rect[4]{0,0,width,height};
 full_copy(f.gpr[2],q.destination.object,0,xy,q.source.object,0,rect,nullptr);
 auto &s=full_state;s=FullPostState{};s.frame=q.stamp.frame;s.context=f.gpr[2];s.armed=true;
 s.render_rect={0,0,static_cast<int>(render_w),static_cast<int>(render_h)};s.full_rect={0,0,static_cast<int>(width),static_cast<int>(height)};
 full_frame.store(q.stamp.frame);++full_counts.final_copies;return true;
}
void full_enter(std::uintptr_t context) noexcept {
 auto &s=full_state;if(!s.armed||context!=s.context)return;
 std::uint64_t frame{};std::array<int,4> rect{};
 if(!field(cfg.renderer,0x1f210,frame)||frame!=s.frame){s.armed=false;++full_counts.stale;return;}
 ++full_counts.schedulers;
 if(!field(context,0x5c,rect)||rect!=s.render_rect){s.armed=false;++full_counts.rect_mismatch;return;}
 s.in_scheduler=true;
}
void full_taa(std::uintptr_t context) noexcept {
 if(std::uint64_t frame{};field(cfg.renderer,0x1f210,frame))full_taa_frame=frame;
 auto &s=full_state;if(!s.in_scheduler||s.switched||context!=s.context)return;
 // The inputs' upscale slot first, in TAA's key. Without it the chain still
 // switches: its depth- and motion-based effects then read the render-size
 // inputs for this frame (a refusal is counted).
 if(const auto refusal=full_slot_record(context,s.frame))++full_counts.slot_refusals[(std::min)(refusal,7U)];
 full_viewport(context,s.full_rect.data());s.switched=true;++full_counts.switches;
}
void full_exit(std::uintptr_t context) noexcept {
 auto &s=full_state;if(!s.in_scheduler||context!=s.context)return;
 if(s.switched||s.tone_map_full){full_viewport(context,s.render_rect.data());++full_counts.restores;}
 if(!s.switched)++full_counts.not_switched;
 s=FullPostState{};
}
// The tone map's callback (unit, context) runs before TAA's. Its final grade
// (key (15, 0x7fffff1), ToneMappingApply_ColorGrading) reuses the texture
// coordinates its exposure step computed from the rectangle at its start
// (game-114: at the render rectangle the grade sampled only that corner of the
// full-size chain, a 1/scale zoom). So the whole callback runs at the full
// rectangle. Its exposure step (key (15, 0), AutoExposureApply*, into TAA's
// input) is per-pixel: no CBScreen, coordinates linear in the pixel position.
// It then also covers pixels outside the render region, which DLSS never
// reads.
void full_tone_map_dispatch(pb::Frame *f){
 const auto context=f->gpr[2];auto &s=full_state;
 const bool mine=s.in_scheduler&&!s.switched&&!s.tone_map_full&&context==s.context;
 if(mine){full_viewport(context,s.full_rect.data());s.tone_map_full=true;++full_counts.tone_map_switches;}
 __try{mhw_binding_forward(originals[7],f);}
 __finally{if(mine){full_viewport(context,s.render_rect.data());s.tone_map_full=false;}}
}
// The projection helper (context, out float4, in position): it returns screen
// coordinates divided by the view's scale product (view +0x6b8), the render
// rectangle's mapping to the output. With the full rectangle they are already
// output coordinates.
void full_project_dispatch(pb::Frame *f){
 const auto context=f->gpr[1];const auto out=f->gpr[2];
 mhw_binding_forward(originals[8],f);
 auto &s=full_state;
 if((s.switched||s.tone_map_full)&&context==s.context&&out){std::uintptr_t view{};float scale{};
  if(field(context,464,view)&&view&&field(view,1720,scale)&&scale>0.F&&scale<1.F){
   auto *v=reinterpret_cast<float *>(out);v[0]*=scale;v[1]*=scale;++full_counts.projections;}}
}
void full_post_write(std::ostream &o){
 o<<"{\"enabled\":"<<full_post_enabled.load()<<",\"final_copies\":"<<full_counts.final_copies<<",\"schedulers\":"<<full_counts.schedulers
  <<",\"switches\":"<<full_counts.switches<<",\"tone_map_switches\":"<<full_counts.tone_map_switches<<",\"projections\":"<<full_counts.projections
  <<",\"restores\":"<<full_counts.restores<<",\"not_switched\":"<<full_counts.not_switched<<",\"rect_mismatch\":"<<full_counts.rect_mismatch
  <<",\"stale\":"<<full_counts.stale<<",\"declined\":"<<full_counts.declined<<",\"no_taa\":"<<full_counts.no_taa<<",\"identity_plans\":"<<full_counts.identity_plans
  <<",\"inputs\":{\"ready\":"<<full_inputs.ready()<<",\"failure\":\""<<full_inputs_failure<<"\",\"bytes\":"<<full_inputs.bytes()<<",\"slots\":"<<full_counts.slots<<",\"planned\":"<<full_counts.slots_planned
  <<",\"upscales\":"<<full_counts.upscales<<",\"unverified\":"<<full_counts.slot_unverified<<",\"stale\":"<<full_counts.slot_stale<<",\"refusals\":[";
 for(unsigned i=0;i<8;++i)o<<(i?",":"")<<full_counts.slot_refusals[i];
 o<<"],\"skips\":[";for(unsigned i=0;i<8;++i)o<<(i?",":"")<<full_counts.slot_skips[i];o<<"]}}";
}

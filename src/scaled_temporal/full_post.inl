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
// A frame's candidate is the final quad (pending); it is armed at the post
// scheduler's entry, once the registration pass has shown TAA's callback will
// run (v0.25.5).
struct FullPostState {
 std::uint64_t frame{};std::uintptr_t context{};
 bool pending{},armed{},in_scheduler{},switched{},tone_map_full{};
 std::array<int,4> render_rect{},full_rect{};
 std::uintptr_t source{},destination{};unsigned key{},order{};  // the final quad's owners and key fields
};
thread_local FullPostState full_state;
std::atomic<std::uint64_t> full_frame{UINT64_MAX};  // the last armed frame (plan builder)
struct FullPostCounters {
 std::atomic<std::uint64_t> final_copies{},schedulers{},switches{},tone_map_switches{},projections{},restores{},
  not_switched{},rect_mismatch{},stale{},declined{},no_taa{},taa_unreadable{},identity_plans{};
 // The inputs' upscale (v0.25.2): slots recorded, refused (by reason),
 // planned, executed with the upscale, executed without it (by reason).
 std::atomic<std::uint64_t> slots{},slots_planned{},upscales{},slot_unverified{},slot_stale{};
 std::array<std::atomic<std::uint64_t>,8> slot_refusals{},slot_skips{};
 // v0.25.4: the inputs' states at the slot come from the game's barrier plan.
 // Checked against TAA's consumer when it had already run, or not (its
 // worker later); the last disagreement: depth proved/planned, GBuffer4
 // proved/planned (under the route lock).
 std::atomic<std::uint64_t> states_checked{},states_planned{};std::array<unsigned,4> state_mismatch{};
 // The late draws' depth restore (full_restore): recorded, skipped (by
 // reason), and restores whose slot then kept nothing (stale).
 std::atomic<std::uint64_t> depth_restores{},restore_unsafe{};std::array<std::atomic<std::uint64_t>,5> restore_skips{};
} full_counts;
// The planner's native state as D3D12 states (the converter 0x259ba50): read
// states are bit sets (1 vertex/constant, 2 index, 4 NPSR, 8 PSR, 0x10
// indirect, 0x20 COPY_SOURCE, 0x40 depth read); 0xff generic read, 0x100
// render target, 0x200 depth write, 0x400 UAV, any other COPY_DEST.
constexpr unsigned planner_d3d12_state(unsigned n) noexcept {
 if(n>=0xff)return n==0xff?2755U:n==0x100?4U:n==0x200?16U:n==0x400?8U:1024U;
 return (n&3)|((n>>1)&0x20)|(16*((n&0xc)|(2*((n&0x10)|(2*(n&0x20))))));
}
constexpr bool planner_state_is(unsigned native,unsigned d3d12) noexcept {return planner_d3d12_state(native)==d3d12;}
static_assert(planner_state_is(4,0x40)&&planner_state_is(8,0x80)&&planner_state_is(12,0xc0)&&planner_state_is(0x20,0x800)&&
 planner_state_is(0x2c,0x8c0)&&planner_state_is(0x40,0x20)&&planner_state_is(0x10,0x200)&&planner_state_is(0x100,4)&&
 planner_state_is(0x200,0x10)&&planner_state_is(0x400,8)&&planner_state_is(2048,0x400)&&planner_state_is(0xff,0xac3),
 "converter 0x259ba50: NPSR, PSR, both, COPY_SOURCE, merged, DEPTH_READ, INDIRECT, RT, DEPTH_WRITE, UAV, COPY_DEST, GENERIC_READ");
// The render-size inputs of the post chain, upscaled at the slot
// (full_post_inputs.hpp). Prepared with the route at the output size.
FullPostInputs full_inputs;std::string full_inputs_failure;
// The upscale slot: an engine copy packet the post scheduler records right
// after TAA's callback, at key (15, 3) -- after TAA's (15, 2) and before every
// later post effect -- into GBuffer5, from TAA's output. The game's barrier
// planner gives that packet GBuffer5 in COPY_DEST; its copy never runs: the
// copy hook records the upscale there instead (full_post_slot.inl), or nothing.
// - The source is the TAA copy's own source, already in COPY_SOURCE at
//   (15, 2): the planner merges consecutive read uses into one state (native
//   read states are bit sets, converter 0x259ba50), so the slot's read joins
//   that span and changes no state.
// - The depth and GBuffer4 are not part of the packet. Read by the slot, either
//   would join its read span with COPY_SOURCE, and TAA's consumer would see
//   that merged state (game-116: GBuffer4 as the source, PSR|NPSR|COPY_SOURCE
//   at TAA, DLSS refused every armed frame). Their states are the ones proven
//   at TAA's consumer, when no planner node of theirs lies up to the slot.
struct FullSlot {
 std::uint64_t frame{};std::uintptr_t context{},packet{};
 tb::Owner depth,gbuffer4,gbuffer5,source;
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
 // TAA's copy (opcode 13 at (15, 2), recorded by the scheduler's first loop):
 // its source, TAA's output, is the slot's.
 struct Command {unsigned key{},padding{};std::uintptr_t packet{};} c;static_assert(sizeof(Command)==16);
 {std::array<Command,256> block;std::uintptr_t source{};unsigned taa_copies{};
  for(unsigned base=0;base<count;base+=256){const auto n=(std::min)(256U,count-base);
   if(!pb::copy_memory(array+16ULL*base,block.data(),n*sizeof(Command)))return 6;
   for(unsigned j=0;j<n;++j){unsigned raw{};if(block[j].key!=0x78000002)continue;
    if(!field(block[j].packet,0,raw))return 6;if((raw&31)==13&&field(block[j].packet,0x18,source))++taa_copies;}}
  if(taa_copies!=1||!tb::read_owner(reader,source,slot.source)||slot.source.width!=width||slot.source.height!=height||slot.source.kind==6||
     slot.source.resource==slot.gbuffer5.resource)return 6;}
 // The game's own order change (as the scheduler makes for each filter), for
 // this one packet; TAA's key is restored right after.
 auto *order=reinterpret_cast<volatile unsigned *>(context+0x154);
 const unsigned xy[2]{},rect[4]{0,0,1,1};
 *order=(key&31)|(3U<<5);full_copy(context,slot.gbuffer5.object,0,xy,slot.source.object,0,rect,nullptr);*order=key;
 // The command array entry the helper appended, found by its owners: it is
 // registered even when it does not verify, so that its copy never runs. Not
 // found, the route stops (fault 15).
 unsigned after{},raw{};std::uintptr_t destination{},source{};
 if(!pb::copy_memory(array+16ULL*count,&c,sizeof(c))||!c.packet||!field(c.packet,0x10,destination)||destination!=slot.gbuffer5.object||
    !field(c.packet,0x18,source)||source!=slot.source.object){fail(15);return 6;}
 slot.packet=c.packet;
 slot.valid=c.key==full_slot_key&&field(context,0x2b80,after)&&after==count+1&&field(c.packet,0,raw)&&(raw&31)==13;
 if(!slot.valid)++full_counts.slot_unverified;
 {Lock lock;auto &q=full_slots[frame%full_slots.size()];if(q.packet&&!q.executed)++full_counts.slot_stale;q=slot;}
 ++full_counts.slots;return slot.valid?0:7;
}
// The depth's and GBuffer4's states at the TAA consumer, as it proved them
// (the depth admission; GBuffer4's transition in TAA's own barriers), published
// by the continuous runtime with its copies for DLSS.
void full_states(const tb::Snapshot &s,unsigned depth_state,std::uintptr_t depth,unsigned gbuffer4_state,std::uintptr_t gbuffer4) noexcept {
 Lock lock;auto &p=plans[s.frame%plans.size()];
 if(p.prepared&&p.stamp==s&&p.full_slot){p.full_depth_state=depth_state;p.full_depth_resource=depth;
  p.full_gbuffer4_state=gbuffer4_state;p.full_gbuffer4_resource=gbuffer4;p.full_states_known=true;}
}
// sCamera::draw records the scene (with the final quad), then runs the post
// filters' registration pass (their callbacks with sFilterSwapchain +57 set add
// their descriptors, sub_1424F2350), then the scheduler. Whether TAA's
// callback runs this frame is known only after the final quad: at a
// transition it may not (game-118: 11 frames armed without the switch, the
// render-size chain under the full copy).
//
// So the final quad stays native and is only remembered here (the candidate:
// its owners, tBaseMap's texture and the view's attachment, and its key).
// slot: the final quad's capture (final_before), entered. The same helper
// (0x2297e20) records other copies too (refraction, other views): those leave
// the candidate as it is (game-119, v0.25.5: they cleared it, and no frame
// armed).
void full_candidate(const pb::Frame &f,unsigned slot){
 if(!full_post_enabled.load()||!full_copy||!full_viewport||!full_inputs.ready()||slot==UINT_MAX||fault.load()||!accepting.load()||(render_w==width&&render_h==height))return;
 FinalQuad q;{Lock lock;q=final_quads[slot];}
 auto &s=full_state;s=FullPostState{};
 if(!q.entered||q.stamp.context!=f.gpr[2]||!field(f.gpr[2],0x154,s.key)||!field(f.gpr[2],0x15c,s.order)){++full_counts.declined;return;}
 s.frame=q.stamp.frame;s.context=f.gpr[2];s.source=q.source.object;s.destination=q.destination.object;s.pending=true;
 s.render_rect={0,0,static_cast<int>(render_w),static_cast<int>(render_h)};s.full_rect={0,0,static_cast<int>(width),static_cast<int>(height)};
}
// TAA's descriptor in this frame's scheduler list (sFilterSwapchain +0x3c
// count, +0x40 descriptors; the unit's vtable 0x351bda8, stage 15, order 2),
// with a callback (descriptor +0x20 clear: the scheduler calls it). At the
// scheduler's entry the list is in registration order, not yet sorted, and
// endpoints without a unit (the colour grade's, (15, 0x7fffff1)) can precede
// TAA's descriptor; they are skipped (game-120: a search that stopped at them
// refused every frame). 0: registered; 1: the list unreadable; 2: not listed.
unsigned full_taa_registered() noexcept {
 std::uintptr_t manager{};unsigned count{};
 if(!field(cfg.image,0x51cc238,manager)||!field(manager,0x3c,count)||count>64)return 1;
 for(unsigned i=0;i<count;++i){std::uintptr_t e{},unit{},type{};unsigned stage{},order{};unsigned char skipped{};
  if(!field(manager,0x40+8*i,e)||!field(e,0,stage)||!field(e,4,order)||!field(e,8,unit)||!field(e,0x20,skipped))return 1;
  if(!unit||skipped||stage!=15||order!=2)continue;
  if(!field(unit,0,type))return 1;
  if(type==cfg.image+0x351bda8)return 0;}
 return 2;
}
// At the scheduler's entry the candidate is armed: the final copy becomes the
// game's DLAA copy (the same owners, the full rectangle), recorded in the
// final quad's key (stage 16, order 0) after the quad, which it overwrites.
// Without TAA's callback this frame, nothing is armed: the frame stays native.
void full_enter(std::uintptr_t context) noexcept {
 auto &s=full_state;if(!s.pending||context!=s.context)return;
 s.pending=false;
 std::uint64_t frame{};std::array<int,4> rect{};
 if(!field(cfg.renderer,0x1f210,frame)||frame!=s.frame){++full_counts.stale;return;}
 ++full_counts.schedulers;
 if(!field(context,0x5c,rect)||rect!=s.render_rect){++full_counts.rect_mismatch;return;}
 if(const auto missing=full_taa_registered()){++(missing==1?full_counts.taa_unreadable:full_counts.no_taa);return;}
 unsigned key{},order{};
 if(!field(context,0x154,key)||!field(context,0x15c,order)){++full_counts.declined;return;}
 auto *k=reinterpret_cast<volatile unsigned *>(context+0x154),*o=reinterpret_cast<volatile unsigned *>(context+0x15c);
 const unsigned xy[2]{},full[4]{0,0,width,height};
 *k=s.key;*o=s.order;full_copy(context,s.destination,0,xy,s.source,0,full,nullptr);*k=key;*o=order;
 s.armed=true;s.in_scheduler=true;full_frame.store(s.frame);++full_counts.final_copies;
}
void full_taa(std::uintptr_t context) noexcept {
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
  <<",\"stale\":"<<full_counts.stale<<",\"declined\":"<<full_counts.declined<<",\"no_taa\":"<<full_counts.no_taa<<",\"taa_unreadable\":"<<full_counts.taa_unreadable<<",\"identity_plans\":"<<full_counts.identity_plans
  <<",\"inputs\":{\"ready\":"<<full_inputs.ready()<<",\"failure\":\""<<full_inputs_failure<<"\",\"bytes\":"<<full_inputs.bytes()<<",\"slots\":"<<full_counts.slots<<",\"planned\":"<<full_counts.slots_planned
  <<",\"upscales\":"<<full_counts.upscales<<",\"states_checked\":"<<full_counts.states_checked<<",\"states_planned\":"<<full_counts.states_planned
  <<",\"state_mismatch\":["<<full_counts.state_mismatch[0]<<','<<full_counts.state_mismatch[1]<<','<<full_counts.state_mismatch[2]<<','<<full_counts.state_mismatch[3]
  <<"],\"depth_restores\":"<<full_counts.depth_restores<<",\"restore_unsafe\":"<<full_counts.restore_unsafe<<",\"restore_skips\":["
  <<full_counts.restore_skips[0]<<','<<full_counts.restore_skips[1]<<','<<full_counts.restore_skips[2]<<','<<full_counts.restore_skips[3]<<','<<full_counts.restore_skips[4]
  <<"],\"unverified\":"<<full_counts.slot_unverified<<",\"stale\":"<<full_counts.slot_stale<<",\"refusals\":[";
 for(unsigned i=0;i<8;++i)o<<(i?",":"")<<full_counts.slot_refusals[i];
 o<<"],\"skips\":[";for(unsigned i=0;i<8;++i)o<<(i?",":"")<<full_counts.slot_skips[i];o<<"]}}";
}

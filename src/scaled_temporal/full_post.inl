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
} full_counts;
// The final quad of a scaled frame becomes the game's DLAA copy (the same
// owners as the quad: tBaseMap's texture into the view's attachment, the full
// rectangle), which arms this thread's post chain for the frame. slot: the
// final quad's capture (final_before), entered.
bool full_final(const pb::Frame &f,unsigned slot){
 if(!full_post_enabled.load()||!full_copy||!full_viewport||slot==UINT_MAX||fault.load()||!accepting.load()||(render_w==width&&render_h==height))return false;
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
  <<",\"stale\":"<<full_counts.stale<<",\"declined\":"<<full_counts.declined<<",\"no_taa\":"<<full_counts.no_taa<<",\"identity_plans\":"<<full_counts.identity_plans<<'}';
}

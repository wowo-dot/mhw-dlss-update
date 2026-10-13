// full_post: the inputs' upscale slot at execution (full_post.inl records it;
// full_post_inputs.hpp records the upscale). Included before the copy hooks.
thread_local unsigned full_slot_scope=UINT_MAX,full_slot_calls{};
thread_local std::uintptr_t full_slot_state{};
// The depth and GBuffer4 are still in the states proven at the TAA consumer:
// the barrier planner's packets after TAA's dispatch, up to and including the
// slot, carry no state node of their backings (node: +0 next, +8 backing, the
// D3D12 resource at backing +0x20). Nested executions (opcode 10) are not
// qualified here.
bool full_inputs_quiet(const Plan &p,const FullSlot &q) noexcept {
 std::uintptr_t array{};unsigned count{};std::uint64_t epoch{};
 if(!field(cfg.renderer,0x7af18,array)||!field(cfg.renderer,0x7af28,count)||!field(cfg.renderer,0x1f208,epoch)||
    epoch!=p.stamp.epoch+1||!array||!count||count>32768||array>UINTPTR_MAX-16ULL*count)return false;
 unsigned taa=UINT_MAX,slot=UINT_MAX,taas{},slots{};std::array<std::array<std::uint64_t,2>,64> block{};
 for(unsigned begin=0;begin<count;begin+=64){const auto n=(std::min)(64U,count-begin);
  if(!pb::copy_memory(array+16ULL*begin,block.data(),n*16))return false;
  for(unsigned i=0;i<n;++i){if(block[i][1]==p.temporal.object){taa=begin+i;++taas;}if(block[i][1]==q.packet){slot=begin+i;++slots;}}}
 if(taas!=1||slots!=1||slot<=taa||slot-taa>64)return false;
 for(unsigned index=taa+1;index<=slot;++index){std::uintptr_t packet{},node{};unsigned raw{},visited{};
  if(!field(array+16ULL*index,8,packet)||!packet||!field(packet,0,raw)||(raw&31)==10||!field(packet,8,node))return false;
  while(node){std::uintptr_t next{},backing{},resource{};
   if(++visited>128||!field(node,0,next)||!field(node,8,backing)||!field(backing,0x20,resource))return false;
   if(backing==q.depth.backing||resource==q.depth.resource||backing==q.gbuffer4.backing||resource==q.gbuffer4.resource)return false;
   node=next;}
 }
 return true;
}
// The copy executor (0x259c650) enters a registered slot packet. The plan's
// hand-off takes the slot's recording here, so the frame completes whether or
// not the upscale is recorded. Returns the slot, or UINT_MAX.
unsigned full_slot_before(const pb::Frame &f) noexcept {
 Lock lock;unsigned found=UINT_MAX;
 for(unsigned i=0;i<full_slots.size();++i)if(full_slots[i].packet&&full_slots[i].packet==f.gpr[3]&&!full_slots[i].executed){found=i;break;}
 if(found==UINT_MAX)return found;
 // A slot whose frame never executed leaves its address to a later packet:
 // only the slot's own copy (TAA's output into GBuffer5, 1x1) is taken.
 {const auto &q=full_slots[found];std::uintptr_t destination{},source{};std::array<unsigned,4> rect{};
  if(!field(q.packet,0x10,destination)||destination!=q.gbuffer5.object||!field(q.packet,0x18,source)||source!=q.source.object||
     !field(q.packet,0x38,rect)||rect!=std::array<unsigned,4>{0,0,1,1}){full_slots[found]={};++full_counts.slot_stale;return UINT_MAX;}}
 auto &q=full_slots[found];q.executed=true;full_slot_calls=0;full_slot_state=0;
 std::uintptr_t state{},list{};
 if(field(f.gpr[1],8,state)&&state&&field(state,0x20,list)&&list)full_slot_state=state;
 auto &p=plans[q.frame%plans.size()];
 if(p.prepared&&p.stamp.frame==q.frame&&p.full_slot==q.packet&&!p.full_slot_done){
  temporal_present::Bindings b;
  if(!full_slot_state||!obs::inspect(reinterpret_cast<ID3D12GraphicsCommandList *>(list),b)||!b.known||!b.valid||!b.generation||
     !track_recording(p,reinterpret_cast<ID3D12GraphicsCommandList *>(list),b.generation,true)||
     !retain_recording(p,reinterpret_cast<ID3D12GraphicsCommandList *>(list),b.generation)||
     p.handoff.record(2,q.packet,{list,b.generation})!=FrameHandoff::Result::ok){fail(15);return found;}
  p.full_slot_done=true;completed(p);
 }
 return found;
}
void full_slot_after() noexcept {if(full_slot_calls!=1)++full_counts.slot_skips[0];}
// The slot's copy (TAA's output into GBuffer5, 1x1) never runs: it is replaced
// by the upscale, or by nothing. GBuffer5 is in COPY_DEST, the planner's state
// for the packet (flushed by the executor just before).
void full_slot_copy(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *destination,UINT x,UINT y,UINT z,
                    const D3D12_TEXTURE_COPY_LOCATION *source,const D3D12_BOX *box,std::uintptr_t caller) noexcept {
 Lock lock;const auto &q=full_slots[full_slot_scope];
 const auto skip=[&](unsigned reason){++full_counts.slot_skips[reason];};
 D3D12_TEXTURE_COPY_LOCATION d{},s{};D3D12_BOX b{};std::uintptr_t packet{},command{};
 if(++full_slot_calls!=1||caller!=cfg.image+0x259caf0||x||y||z||!full_slot_state||
    !field(full_slot_state,0x158,packet)||packet!=q.packet||!field(full_slot_state,0x20,command)||command!=reinterpret_cast<std::uintptr_t>(list)||
    !pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination),&d,sizeof(d))||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(source),&s,sizeof(s))||
    d.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||s.Type!=d.Type||d.SubresourceIndex||s.SubresourceIndex||
    reinterpret_cast<std::uintptr_t>(d.pResource)!=q.gbuffer5.resource||reinterpret_cast<std::uintptr_t>(s.pResource)!=q.source.resource||
    !box||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(box),&b,sizeof(b))||b.left||b.top||b.front||b.right!=1||b.bottom!=1||b.back!=1){skip(1);return;}
 if(!q.valid||fault.load()){skip(2);return;}
 const auto &p=plans[q.frame%plans.size()];
 if(!p.prepared||p.stamp.frame!=q.frame||p.full_slot!=q.packet||!p.full_slot_done){skip(3);return;}
 if(!p.full_states_known||p.full_depth_resource!=q.depth.resource||p.full_gbuffer4_resource!=q.gbuffer4.resource){skip(4);return;}
 if(!full_inputs_quiet(p,q)){skip(5);return;}
 temporal_present::Bindings roots;obs::Predicate predicate;
 if(!obs::inspect(list,roots)||!roots.known||!roots.valid||!roots.generation||!obs::predicate_current(list,predicate)||!predicate.known||predicate.buffer){skip(6);return;}
 const std::array<FullPostInput,3> inputs{{{reinterpret_cast<ID3D12Resource *>(q.depth.resource),static_cast<D3D12_RESOURCE_STATES>(p.full_depth_state)},
  {reinterpret_cast<ID3D12Resource *>(q.gbuffer4.resource),static_cast<D3D12_RESOURCE_STATES>(p.full_gbuffer4_state)},
  {reinterpret_cast<ID3D12Resource *>(q.gbuffer5.resource),D3D12_RESOURCE_STATE_COPY_DEST}}};
 injecting=true;const bool recorded=full_inputs.record(list,inputs,render_w,render_h,roots);injecting=false;
 if(!recorded){skip(7);return;}
 // Everything but an unset pipeline is the game's again.
 temporal_present::Bindings after;
 if(!obs::inspect(list,after)||!(after.compute==roots.compute)||!(after.graphics==roots.graphics)||after.heaps!=roots.heaps||
    after.heap_count!=roots.heap_count||(roots.pipeline&&after.pipeline!=roots.pipeline)){fail(15);return;}
 ++full_counts.upscales;
}

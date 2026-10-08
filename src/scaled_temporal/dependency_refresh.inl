// Called under the route lock at native draw entry, before native SRV binding.
// Preparation's scene dependency graph is immutable. Only auxiliary storage
// behind the SAME owner may change; old resources remain pinned in the plan.
struct DependencyRefresh {
 std::uint64_t entries{},unchanged{},refreshed{},rejected{},api_checks{},api_rejected{};
 unsigned reason{};std::array<std::uint64_t,7> last{}; // frame,stage,register,old/new width,old/new height
 void write(std::ostream &o)const{
  o<<"{\"schema\":1,\"policy\":\"auxiliary-entry-retention-v1\",\"entries\":"<<entries
   <<",\"unchanged\":"<<unchanged<<",\"refreshed\":"<<refreshed<<",\"rejected\":"<<rejected
   <<",\"api_checks\":"<<api_checks<<",\"api_rejected\":"<<api_rejected<<",\"reason\":"<<reason<<",\"last\":[";
  for(unsigned i=0;i<last.size();++i){if(i)o<<',';o<<last[i];}o<<"]}";
 }
} dependency_refresh;
bool dependency_auxiliary_change(const Record &r,const HazeBindings &before,const HazeBindings &now){
 if(r.dependency!=2)return before==now;
 auto stable=now;
 for(unsigned j=1;j<3;++j){auto &o=stable.textures[j];const auto &old=before.textures[j];
  // Identity, kind, flags, type and formats cannot change. t0, constants,
  // samplers, binding layout and all non-texture fields stay exact.
  o.width=old.width;o.height=old.height;o.primary=old.primary;o.secondary=old.secondary;o.backing=old.backing;o.resource=old.resource;
  stable.views[j]=before.views[j];stable.descriptors[j]=before.descriptors[j];
 }
 return stable==before;
}
bool dependency_enter(Plan &p,unsigned stage){
 auto &r=p.draws[stage];r.binding_ready=false;++dependency_refresh.entries;
 const auto reject=[&](unsigned reason){++dependency_refresh.rejected;dependency_refresh.reason=reason;return false;};
 const auto &before=p.dependency_bindings[stage-4];HazeBindings now{};
 if(!dependency_bindings_read(r,now))return reject(1);
 if(!dependency_auxiliary_change(r,before,now))return reject(2);
 if(now==before){p.dependency_current[stage-4]=now;r.binding_ready=true;++dependency_refresh.unchanged;return true;}
 std::array<ComPtr<ID3D12Resource>,2> held;auto indices=r.reads;unsigned next=resource_count(p);
 for(unsigned j=1;j<3;++j){
  if(now.textures[j]==before.textures[j]&&now.views[j]==before.views[j]&&now.descriptors[j]==before.descriptors[j])continue;
  if(!dependency_texture(now.textures[j],j+1,held[j-1]))return reject(3);
  const auto id=now.textures[j].resource;
  // Auxiliary inputs may not acquire scene/output provenance or alias a write.
  for(unsigned k=0;k<(p.has_haze?9U:5U);++k)if(p.resources[k].Get()==held[j-1].Get())return reject(4);
  for(unsigned k=0;k<effect_count(p);++k){const auto &other=p.draws[effect_stage(p,k)];
   if(other.output.resource==id||(other.dependency&&p.dependency_bindings[effect_stage(p,k)-4].textures[0].resource==id))return reject(4);
  }
  unsigned index{};for(;index<resource_count(p);++index)if(p.resources[index]==held[j-1])break;
  if(index==resource_count(p)){
   if(j==2&&held[0]&&held[0]==held[1])index=indices[1];
   else {if(next==resource_capacity)return reject(5);index=next++;}
  }
  indices[j]=index;
 }
 // No partial snapshot publication. Re-read after pinning and descriptor checks.
 HazeBindings again{};if(!dependency_bindings_read(r,again)||again!=now)return reject(6);
 const auto old_count=resource_count(p);
 for(unsigned j=1;j<3;++j)if(held[j-1]&&indices[j]>=old_count)p.resources[indices[j]]=held[j-1];
 p.extra_resources+=next-old_count;
 if(next!=old_count)watch(static_cast<unsigned>(p.stamp.frame%plans.size()),p);  // perf::packet_filter barriers
 for(unsigned i=old_count;i<next;++i)for(auto &track:p.recordings)if(track.list){
  const auto state=recording_states.get(reinterpret_cast<std::uintptr_t>(track.list),track.generation,reinterpret_cast<std::uintptr_t>(p.resources[i].Get()));
  track.known[i]=state.known;track.states[i]=state.value;if(state.known)++state_imports;
 }
 r.reads=indices;p.dependency_current[stage-4]=now;r.binding_ready=true;++dependency_refresh.refreshed;
 for(unsigned j=1;j<3;++j)if(held[j-1])dependency_refresh.last={p.stamp.frame,stage,j,before.textures[j].width,now.textures[j].width,before.textures[j].height,now.textures[j].height};
 return true;
}
bool dependency_current_stable(const Plan &p,unsigned stage){
 const auto &r=p.draws[stage];++dependency_refresh.api_checks;
 const auto check=dependency_binding_check(p,stage,r.binding_ready?&p.dependency_current[stage-4]:nullptr);
 dependency_binding_trace.draw({GetTickCount64(),p.stamp.frame,p.stamp.epoch,stage,r.dependency,r.draw.packet,r.packet_bindings[2],r.draw.shader_set},r.binding_entry,check);
 const bool accepted=r.binding_ready&&check.accepted();if(!accepted)++dependency_refresh.api_rejected;
 return accepted;
}

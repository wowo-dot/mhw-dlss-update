// Included inside provenance's private namespace after its copy observer.
unsigned readback_stages() noexcept;
using ReadbackHistory=std::array<const Event *,history_capacity>;
static_assert(sizeof(ReadbackHistory)<=64U*1024U);
std::array<depth_readback::Capture,sequence_samples> depth_pixels;
bool readback_enabled{};
thread_local bool readback_internal{};
struct InternalReadback {InternalReadback(){readback_internal=true;}~InternalReadback(){readback_internal=false;}};

// Only published immutable history is read. Unpublished reservation headers
// reject this opportunity; pending calls on another list do not block it.
bool readback_recording(const Event &current,ReadbackHistory &calls,unsigned &count) noexcept {
  count=0;const auto n=(std::min)(history_count.load(),history_capacity);
  for(unsigned i=0;i<n;++i) {
    const auto state=history[i].published.load(std::memory_order_acquire);
    if(!state)return false;
    const auto &e=history[i].value;
    if(e.object!=current.object || e.enter==current.enter || (e.kind>=execute && e.kind<=fence_signal))continue;
    if(state!=2 || !e.valid || !e.returned || e.exit>=current.enter)return false;
    calls[count++]=&e;
  }
  std::sort(calls.begin(),calls.begin()+count,[](auto a,auto b){return a->enter<b->enter;});
  return true;
}
bool readback_view() noexcept {
  std::uintptr_t handle{},resource{};unsigned char ro{};
  if(!read(marker.view+0x10,handle) || handle!=marker.handle || !read(marker.view+0x18,resource) || resource!=marker.resource ||
     !read(marker.view+0x20,ro) || ro)return false;
  const Event *ctor{},*dsv{};unsigned parent_index{};
  const auto n=(std::min)(view_count.load(),watch_capacity);
  for(unsigned i=0;i<n;++i) {
    if(views[i].published.load(std::memory_order_acquire)!=2)return false;
    const auto &e=views[i].value;if(!e.valid || !e.returned)return false;
    if(e.kind==construct && e.object==marker.view && (!ctor || e.enter>ctor->enter)){ctor=&e;parent_index=i+1;}
  }
  if(!ctor || ctor->exit>=marker.order || ctor->handle!=marker.handle || ctor->resource!=marker.resource)return false;
  for(unsigned i=0;i<n;++i) {
    const auto &e=views[i].value;
    if(e.kind==create_dsv && e.parent==parent_index && e.handle==marker.handle && e.resource==marker.resource &&
       e.identity==marker.identity && !e.null_desc && e.descriptor==std::array<unsigned,6>{40,3,0,0,0,0})dsv=&e;
    if(e.enter>ctor->exit) {
      if((e.kind==destroy && e.object==marker.view) || (e.kind==create_dsv && e.handle==marker.handle))return false;
      if(e.kind==copy_descriptors || e.kind==copy_simple)for(unsigned j=0;j<e.count;++j)
        if(marker.handle>=e.ranges[j].start && marker.handle-e.ranges[j].start<std::uint64_t(e.ranges[j].count)*cfg.stride)return false;
    }
  }
  return dsv!=nullptr;
}
void readback_copy(const Event &e) noexcept {
  unsigned stage{};const bool tagged=frame_copy(e,stage);
  auto &pixels=depth_pixels[stage];
  if(!tagged || !readback_enabled || pixels.phase()!=depth_readback::Phase::ready || !healthy() ||
     e.kind!=copy_resource || !e.valid || !e.returned || !anchors())return;
  if(!cfg.same_frame_readback && (!draw_return.load(std::memory_order_acquire) || !selected_command.load(std::memory_order_acquire) ||
     e.resource!=marker.resource || e.enter<=marker.order))return;
  const auto &source=copy_pins[e.first*2],&destination=copy_pins[e.first*2+1];
  if(source.ready!=1 || destination.ready!=1 || (!cfg.same_frame_readback && source.identity!=marker.identity) || !pixels.source_matches(source.desc))return;
  const auto &d=destination.desc;
  if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.Width!=source.desc.Width || d.Height!=source.desc.Height ||
     d.DepthOrArraySize!=1 || d.MipLevels!=1 || d.Format!=DXGI_FORMAT_R32_FLOAT || d.SampleDesc.Count!=1 || d.SampleDesc.Quality ||
     d.Layout!=D3D12_TEXTURE_LAYOUT_UNKNOWN || d.Flags!=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)return;
  ReadbackHistory calls{};unsigned count{};
  if(!readback_recording(e,calls,count) || !count)return;
  const Event *reset{};
  for(unsigned i=0;i<count;++i)if(calls[i]->kind==list_reset)reset=calls[i];
  if(!reset || reset->result>=0x80000000 || !reset->resource)return;
  const Event *last{};
  for(unsigned i=0;i<count;++i)if(calls[i]->enter>=reset->enter) {
    const auto &c=*calls[i];
    if(last && last->exit>=c.enter)return;
    if(c.kind==close || c.kind==bundle)return;
    last=&c;
  }
  if(!last || last->kind!=resource_barrier || last->exit>=e.enter)return;
  unsigned source_transitions{},destination_transitions{};
  for(unsigned i=0;i<last->count;++i) {
    const auto &b=barrier_data[last->first+i];
    if(b.type==1 && (!b.resource || !b.other || b.resource==e.resource || b.other==e.resource || b.resource==e.backing || b.other==e.backing))return;
    if(b.type==0 && (b.resource==e.resource || b.resource==e.backing)) {
      if(b.flags || b.subresource!=UINT_MAX)return;
      if(b.resource==e.resource){if(b.before!=0x10 || b.after!=0x800)return;++source_transitions;}
      else {if(b.before!=0xc0 || b.after!=0x400)return;++destination_transitions;}
    }
  }
  if(source_transitions!=1 || destination_transitions!=1 || (!cfg.same_frame_readback && !readback_view()))return;
  depth_readback::Site site{};
  // The same-frame copy can be recorded before the selected draw on another
  // worker. The final draw/view identity is mandatory before private Signal/Map.
  if(cfg.same_frame_readback)site.selection=selected_source_order(stage);else site.marker=marker.order;
  site.request_control=cfg.triplet_readback && stage!=1;
  site.resource=e.resource;site.identity=source.identity;site.command=e.object;
  site.reset=reset->enter;site.allocator=reset->resource;site.barrier=last->enter;site.copy=e.enter;site.copy_exit=e.exit;
  site.begin=order.fetch_add(1)+1;
  InternalReadback internal;
  if(pixels.issue(reinterpret_cast<ID3D12GraphicsCommandList *>(e.object),reinterpret_cast<ID3D12Resource *>(e.resource),site))
    pixels.issue_end(order.fetch_add(1)+1);
  else if(pixels.terminal())fail(Error::read);
}
void readback_submit_one(const Event &e,unsigned stage) noexcept {
  auto &pixels=depth_pixels[stage];
  if(!readback_enabled || pixels.phase()!=depth_readback::Phase::recorded || e.kind!=execute || !e.valid || !e.returned)return;
  auto site=pixels.site();bool contains=false;
  for(unsigned i=0;i<e.count;++i)contains=contains || submitted[e.first+i]==site.command;
  if(!contains)return;
  if(cfg.same_frame_readback && !site.request_control) {
    if(!draw_return.load(std::memory_order_acquire) || !selected_command.load(std::memory_order_acquire) ||
       !selected_submission.load(std::memory_order_acquire) || site.resource!=marker.resource || site.identity!=marker.identity ||
       !readback_view()){fail(Error::changed);return;}
    const Event *draw_submit{};
    for(unsigned i=0;i<(std::min)(history_count.load(),history_capacity);++i)
      if(history[i].published.load(std::memory_order_acquire)==2 && history[i].value.enter==selected_submission.load())draw_submit=&history[i].value;
    if(!draw_submit || draw_submit->object!=e.object || draw_submit->enter>e.enter ||
       (draw_submit->enter!=e.enter && draw_submit->exit>=e.enter)){fail(Error::changed);return;}
    if(draw_submit->enter==e.enter) {
      unsigned draw_index=UINT_MAX,copy_index=UINT_MAX;
      for(unsigned i=0;i<e.count;++i){if(submitted[e.first+i]==marker.command)draw_index=i;if(submitted[e.first+i]==site.command)copy_index=i;}
      if(draw_index>=copy_index){fail(Error::changed);return;}
    }
    site.marker=marker.order;
  }
  // Association is checked AFTER the native Execute returns. A failed check
  // cannot erase a recorded copy; the allocation remains pinned and unmapped.
  auto current=e;current.object=site.command;
  ReadbackHistory calls{};unsigned count{};
  if(!healthy() || !readback_recording(current,calls,count) || !count){fail(Error::changed);return;}
  const Event *reset{},*closed{};
  for(unsigned i=0;i<count;++i)if(calls[i]->kind==list_reset)reset=calls[i];
  if(!reset || reset->enter!=site.reset){fail(Error::changed);return;}
  const Event *last{};unsigned closes{};
  for(unsigned i=0;i<count;++i)if(calls[i]->enter>=reset->enter) {
    const auto &c=*calls[i];if(last && last->exit>=c.enter){fail(Error::changed);return;}
    if(c.kind==close){closed=&c;++closes;}if(c.kind==bundle){fail(Error::changed);return;}last=&c;
  }
  if(closes!=1 || closed!=last || closed->result>=0x80000000 || closed->enter<=site.end){fail(Error::changed);return;}
  site.close=closed->enter;site.execute=e.enter;site.execute_exit=e.exit;site.queue=e.object;site.signal_begin=order.fetch_add(1)+1;
  InternalReadback internal;
  if(pixels.submit(reinterpret_cast<ID3D12CommandQueue *>(e.object),site))pixels.signal_end(order.fetch_add(1)+1);
  else fail(Error::read);
}
void readback_submit(const Event &e) noexcept {
  for(unsigned stage=0;stage<readback_stages();++stage)if(stage==1 || cfg.triplet_readback)readback_submit_one(e,stage);
}
void readback_retirement(depth_readback::Capture &pixels) noexcept {
  const auto phase=pixels.phase();
  if(phase!=depth_readback::Phase::submitted && phase!=depth_readback::Phase::complete)return;
  const auto site=pixels.submitted_site();const Event *reset{};
  const auto n=(std::min)(history_count.load(),history_capacity);
  // An unfinished Execute may contain this list; do not map while its payload
  // and return interval are still unpublished. No render thread waits here.
  for(unsigned i=0;i<n;++i) {
    const auto state=history[i].published.load(std::memory_order_acquire);
    if(!state)return;
    const auto &e=history[i].value;
    if(state!=2) {if(e.kind==execute || e.object==site.command)return;continue;}
    if(e.kind==list_reset && e.object==site.command && e.enter>site.execute_exit && (!reset || e.enter<reset->enter))reset=&e;
  }
  if(!reset)return;
  if(!reset->valid || !reset->returned || reset->result>=0x80000000){fail(Error::changed);return;}
  for(unsigned i=0;i<n;++i)if(history[i].published.load(std::memory_order_acquire)==2) {
    const auto &e=history[i].value;
    if(e.kind!=execute || e.enter==site.execute || e.enter>=reset->enter || e.exit<=site.reset)continue;
    if(!e.valid || !e.returned || !e.count || e.count>batch_capacity || e.first>submission_capacity || e.count>submission_capacity-e.first){fail(Error::changed);return;}
    for(unsigned j=0;j<e.count;++j)if(submitted[e.first+j]==site.command){fail(Error::changed);return;}
  }
  pixels.retire(reset->enter,reset->exit);
}

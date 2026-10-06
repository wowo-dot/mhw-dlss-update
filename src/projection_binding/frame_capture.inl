// Bounded packet receipts. Included in provenance's private namespace.
// A producer stamp is never replaced by a later sample from a worker thread.
constexpr unsigned frame_capacity=256;
struct FrameStamp {
  std::uint64_t frame{},epoch{};unsigned bank{},async_mode{};
  bool operator==(const FrameStamp &) const = default;
};
struct DepthPacket {
  unsigned opcode{},kind{};
  std::uintptr_t destination{},source{},nested{};
  std::array<unsigned,6> rectangle{};
  bool operator==(const DepthPacket &) const = default;
};
struct FrameReceipt {
  std::uint64_t enter{},exit{};unsigned thread{},before_count{},after_count{};
  std::uintptr_t context{},caller{},packet{},destination{},source{},before_array{},after_array{};
  FrameStamp stamp;DepthPacket data;
  bool returned{},valid{};
};
struct FrameConsumer {
  std::uint64_t enter{},exit{},copy{},copy_exit{};unsigned thread{},receipt{},native_calls{};
  std::uintptr_t executor{},context{},packet{},command{},source_resource{},destination_resource{};
  DepthPacket data;bool returned{},valid{};
};
template<class T>struct FrameSlot {std::atomic<unsigned> published{};T value;};
std::array<FrameSlot<FrameReceipt>,frame_capacity> frame_receipts;
std::array<FrameSlot<FrameConsumer>,frame_capacity> frame_consumers;
std::atomic<unsigned> frame_receipt_count{},frame_consumer_count{};
struct FrameSelection {
  std::uint64_t order{},core_order{};std::uintptr_t renderer{},context{},constants{};
  unsigned thread{},buffer_id{};FrameStamp stamp;
};
std::array<FrameSlot<FrameSelection>,sequence_samples> frame_selections;
unsigned readback_stages() noexcept {return cfg.sequence_readback?sequence_samples:3U;}
auto &frame_selection=frame_selections[1].value;
auto &frame_selection_state=frame_selections[1].published;
thread_local unsigned frame_parent{};
struct FrameToken {unsigned index{UINT_MAX};bool admitted{},producer{};};

bool frame_stamp(FrameStamp &s) noexcept {
  const auto r=cfg.frame_renderer;std::uintptr_t current{};unsigned char async{};
  return r && r<=UINTPTR_MAX-0x1f218 && read(cfg.image+0x51c4480,current) && current==r &&
    read(r+0x1f210,s.frame) && read(r+0x1f208,s.epoch) && read(r+0x1f200,s.bank) && s.bank<=1 &&
    read(r+0x93,async) && async<=1 && ((s.async_mode=async),true);
}
bool depth_packet(std::uintptr_t at,DepthPacket &p) noexcept {
  unsigned raw{};
  return at && at<=UINTPTR_MAX-0x48 && read(at,raw) && ((p.opcode=raw&31),p.opcode==10) &&
    read(at+0x10,p.kind) && p.kind==8 && read(at+0x18,p.destination) && p.destination &&
    read(at+0x20,p.source) && p.source && p.source!=p.destination &&
    copy_memory(at+0x28,p.rectangle.data(),24) && read(at+0x40,p.nested);
}
bool frame_owner_resource(std::uintptr_t owner,std::uintptr_t &resource) noexcept {
  std::uintptr_t backing{};
  return owner && owner<=UINTPTR_MAX-0xf8 && read(owner+0xf0,backing) &&
    (backing || read(owner+0x60,backing)) && backing && backing<=UINTPTR_MAX-0x28 &&
    read(backing+0x20,resource) && resource;
}
FrameToken frame_before(unsigned kind,const Frame &f) noexcept {
  FrameToken t;t.producer=kind==15;
  if(!cfg.frame_renderer || readback_internal || !frame_open.load(std::memory_order_acquire) || !healthy())return t;
  if(t.producer && (f.gpr[1]!=cfg.frame_renderer+0x710 || static_cast<unsigned>(f.stack[0])!=8))return t;
  if(t.producer) {
    std::array<unsigned,4> rect{};std::uintptr_t view{},owner{};
    if(!copy_memory(f.gpr[2],rect.data(),16) || rect[0]==rect[2] || rect[1]==rect[3])return t;
    if(!read(f.gpr[1]+0x50,view) || !view || view>UINTPTR_MAX-0x60 || !read(view+0x58,owner) || owner==f.gpr[4])return t;
  }
  if(!t.producer) {
    // Other resource-copy kinds and fallback draws remain outside this receipt.
    unsigned raw{},type{};
    if(!f.gpr[3] || f.gpr[3]>UINTPTR_MAX-0x48 || !read(f.gpr[3],raw) || (raw&31)!=10 ||
       !read(f.gpr[3]+0x10,type) || type!=8)return t;
  }
  if(!enter())return t;t.admitted=true;
  if(!anchors() || GetTickCount64()-frame_started>=window_ms){fail(Error::changed);return t;}
  t.index=(t.producer?frame_receipt_count:frame_consumer_count).fetch_add(1);
  if(t.index>=frame_capacity){fail(Error::capacity);return t;}
  if(t.producer) {
    auto &s=frame_receipts[t.index];auto &r=s.value;
    r.enter=order.fetch_add(1)+1;r.thread=GetCurrentThreadId();r.context=f.gpr[1];r.caller=f.caller;r.destination=f.gpr[4];
    std::uintptr_t view{};
    r.valid=frame_stamp(r.stamp) && read(r.context+0x2b80,r.before_count) && r.before_count<65536 &&
      read(r.context+0x2b78,r.before_array) && r.before_array && read(r.context+0x50,view) && view &&
      view<=UINTPTR_MAX-0x60 && read(view+0x58,r.source) && r.source && r.destination && r.source!=r.destination;
    s.published.store(1,std::memory_order_release);
    if(!r.valid)fail(Error::read);
  } else {
    auto &s=frame_consumers[t.index];auto &r=s.value;
    r.enter=order.fetch_add(1)+1;r.thread=GetCurrentThreadId();r.executor=f.gpr[1];r.context=f.gpr[2];r.packet=f.gpr[3];
    std::uintptr_t state{},backend{};
    r.valid=!frame_parent && r.executor && r.executor<=UINTPTR_MAX-16 && r.context && r.context<=UINTPTR_MAX-16 &&
      read(r.context+8,backend) && backend==cfg.backend && read(r.executor+8,state) && state && state<=UINTPTR_MAX-0x28 &&
      read(state+0x20,r.command) && r.command && depth_packet(r.packet,r.data) &&
      frame_owner_resource(r.data.source,r.source_resource) && frame_owner_resource(r.data.destination,r.destination_resource);
    if(r.valid) {
      const auto n=(std::min)(frame_receipt_count.load(),frame_capacity);
      // Only completed receipts are immutable. Select the latest generation at
      // this address; replay additionally checks reuse and overlap intervals.
      for(unsigned i=0;i<n;++i)if(frame_receipts[i].published.load(std::memory_order_acquire)==2) {
        const auto &p=frame_receipts[i].value;
        if(p.valid && p.returned && p.packet==r.packet && p.exit<r.enter &&
           (!r.receipt || p.enter>frame_receipts[r.receipt-1].value.enter)) {
          r.receipt=i+1;
        }
      }
    }
    if(r.receipt && !(frame_receipts[r.receipt-1].value.data==r.data))r.receipt=0;
    s.published.store(1,std::memory_order_release);
    if(!r.valid)fail(Error::read);else frame_parent=t.index+1;
  }
  return t;
}
void frame_after(FrameToken t,bool returned) noexcept {
  if(!t.admitted)return;
  if(t.index<frame_capacity) {
    if(t.producer) {
      auto &s=frame_receipts[t.index];auto &r=s.value;FrameStamp after;
      r.returned=returned;
      if(r.valid && returned) {
        r.valid=frame_stamp(after) && after==r.stamp && read(r.context+0x2b80,r.after_count) &&
          r.after_count>r.before_count && r.after_count<=65536 && r.after_count-r.before_count<=64 &&
          read(r.context+0x2b78,r.after_array) && r.after_array && r.after_array<=UINTPTR_MAX-16ULL*r.after_count &&
          read(r.after_array+16ULL*(r.after_count-1)+8,r.packet) && depth_packet(r.packet,r.data) &&
          r.data.source==r.source && r.data.destination==r.destination;
        FrameStamp stable;DepthPacket verify;unsigned count{};std::uintptr_t base{},packet{};
        r.valid=r.valid && frame_stamp(stable) && stable==r.stamp && depth_packet(r.packet,verify) && verify==r.data &&
          read(r.context+0x2b80,count) && count==r.after_count && read(r.context+0x2b78,base) && base==r.after_array &&
          read(base+16ULL*(count-1)+8,packet) && packet==r.packet;
      } else r.valid=false;
      r.exit=order.fetch_add(1)+1;s.published.store(2,std::memory_order_release);
      if(!r.valid)fail(returned?Error::changed:Error::exception);
    } else {
      auto &s=frame_consumers[t.index];auto &r=s.value;DepthPacket after;
      r.returned=returned;r.valid=r.valid && returned && depth_packet(r.packet,after) && after==r.data;
      r.exit=order.fetch_add(1)+1;s.published.store(2,std::memory_order_release);
      if(frame_parent==t.index+1)frame_parent=0;
      if(!r.valid)fail(returned?Error::changed:Error::exception);
    }
  }
  gate.fetch_sub(1,std::memory_order_release);
}
bool frame_copy(const Event &e,unsigned &stage) noexcept {
  stage=1;
  if(!cfg.frame_renderer)return true;
  if(!frame_parent || frame_parent>frame_capacity)return false;
  auto &r=frame_consumers[frame_parent-1].value;DepthPacket verify;
  ++r.native_calls;
  const bool match=r.native_calls==1 && r.valid && r.receipt && r.command==e.object &&
    r.source_resource==e.resource && r.destination_resource==e.backing &&
    depth_packet(r.packet,verify) && verify==r.data && r.enter<e.enter && e.valid && e.returned;
  if(match){r.copy=e.enter;r.copy_exit=e.exit;}
  if(!match || !cfg.same_frame_readback)return match;
  const auto &p=frame_receipts[r.receipt-1].value;
  for(unsigned i=0;i<readback_stages();++i) {
    if((i!=1 && !cfg.triplet_readback) || frame_selections[i].published.load(std::memory_order_acquire)!=2)continue;
    const auto &s=frame_selections[i].value;
    if(p.stamp==s.stamp && p.context==s.context && p.thread==s.thread && s.order<p.enter){stage=i;return true;}
  }
  return false;
}
bool frame_select(const Source &s,unsigned stage) noexcept {
  if(!cfg.same_frame_readback)return true;
  if(stage>=readback_stages() || (!cfg.triplet_readback && stage!=1) || !enter())return false;
  auto &selection=frame_selections[stage];
  unsigned expected{};FrameStamp now;
  const FrameStamp wanted{s.anchor.frame,s.anchor.epoch,s.anchor.bank,s.anchor.async_mode};
  const bool valid=selection.published.compare_exchange_strong(expected,1) &&
    frame_open.load(std::memory_order_acquire) && healthy() && s.valid && s.projection.valid &&
    s.anchor.renderer==cfg.frame_renderer && s.anchor.context==cfg.frame_renderer+0x710 &&
    s.order && s.thread==GetCurrentThreadId() && s.projection.constants &&
    anchors() && frame_stamp(now) && now==wanted;
  if(valid) {
    selection.value={order.fetch_add(1)+1,s.order,s.anchor.renderer,s.anchor.context,s.projection.constants,
      s.thread,s.projection.buffer_id,wanted};
    selection.published.store(2,std::memory_order_release);
  } else fail(Error::changed);
  gate.fetch_sub(1,std::memory_order_release);return valid;
}
std::uint64_t selected_source_order(unsigned stage) noexcept {
  return frame_selections[stage].published.load(std::memory_order_acquire)==2?frame_selections[stage].value.order:0;
}
void frame_json(std::ostream &o) {
  if(!cfg.frame_renderer)return;
  auto packet=[&](const DepthPacket &p) {
    o<<"{\"opcode\":"<<p.opcode<<",\"kind\":"<<p.kind<<",\"source\":"<<p.source<<",\"destination\":"<<p.destination
      <<",\"nested\":"<<p.nested<<",\"rectangle\":[";
    for(unsigned i=0;i<6;++i){if(i)o<<',';o<<p.rectangle[i];}o<<"]}";
  };
  o<<",\"frame_packets\":{\"schema\":1,\"capacity\":"<<frame_capacity<<",\"renderer\":"<<cfg.frame_renderer
    <<",\"receipts_reserved\":"<<frame_receipt_count.load()<<",\"consumers_reserved\":"<<frame_consumer_count.load()<<",\"receipts\":[";
  for(unsigned i=0;i<(std::min)(frame_receipt_count.load(),frame_capacity);++i) {
    if(i)o<<',';const auto &s=frame_receipts[i];const auto &r=s.value;
    o<<"{\"published\":"<<s.published.load()<<",\"enter\":"<<r.enter<<",\"exit\":"<<r.exit<<",\"thread\":"<<r.thread
      <<",\"context\":"<<r.context<<",\"caller\":"<<r.caller<<",\"packet\":"<<r.packet<<",\"source\":"<<r.source
      <<",\"destination\":"<<r.destination<<",\"before_count\":"<<r.before_count<<",\"after_count\":"<<r.after_count
      <<",\"before_array\":"<<r.before_array<<",\"after_array\":"<<r.after_array<<",\"frame\":"<<r.stamp.frame
      <<",\"epoch\":"<<r.stamp.epoch<<",\"bank\":"<<r.stamp.bank<<",\"async_mode\":"<<r.stamp.async_mode
      <<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"data\":";packet(r.data);o<<'}';
  }
  o<<"],\"consumers\":[";
  for(unsigned i=0;i<(std::min)(frame_consumer_count.load(),frame_capacity);++i) {
    if(i)o<<',';const auto &s=frame_consumers[i];const auto &r=s.value;
    o<<"{\"published\":"<<s.published.load()<<",\"enter\":"<<r.enter<<",\"exit\":"<<r.exit<<",\"thread\":"<<r.thread
      <<",\"executor\":"<<r.executor<<",\"context\":"<<r.context<<",\"packet\":"<<r.packet<<",\"command\":"<<r.command
      <<",\"receipt\":"<<r.receipt<<",\"native_calls\":"<<r.native_calls<<",\"copy\":"<<r.copy<<",\"copy_exit\":"<<r.copy_exit
      <<",\"source_resource\":"<<r.source_resource<<",\"destination_resource\":"<<r.destination_resource
      <<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"data\":";packet(r.data);o<<'}';
  }
  o<<']';
  if(cfg.same_frame_readback) {
    auto selection_json=[&](unsigned stage) {const auto &s=frame_selections[stage].value;
    o<<",\"selection\":{\"published\":"<<frame_selections[stage].published.load()<<",\"order\":"<<s.order<<",\"core_order\":"<<s.core_order
      <<",\"renderer\":"<<s.renderer<<",\"context\":"<<s.context<<",\"constants\":"<<s.constants
      <<",\"thread\":"<<s.thread<<",\"buffer_id\":"<<s.buffer_id<<",\"frame\":"<<s.stamp.frame
      <<",\"epoch\":"<<s.stamp.epoch<<",\"bank\":"<<s.stamp.bank<<",\"async_mode\":"<<s.stamp.async_mode<<'}';};
    selection_json(1);
    if(cfg.triplet_readback) {
      o<<",\"controls\":[";
      for(unsigned stage=0;stage<readback_stages();++stage)if(stage!=1) {if(stage)o<<',';o<<"{\"stage\":"<<stage;selection_json(stage);o<<'}';}
      o<<']';
    }
  }
  o<<'}';
}

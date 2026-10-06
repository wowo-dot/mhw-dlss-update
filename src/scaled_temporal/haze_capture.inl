// Included inside stream_route's anonymous namespace. CPU observation only,
// serialized by its mutex; no COM ownership, GPU calls, or route admission.
struct HazeSample {
 tb::Snapshot stamp;tb::Owner source,destination;
 std::uintptr_t unit{},queue{},packet{};unsigned before{},after{};
 std::uint64_t tick{};
 std::array<unsigned,4> rectangle{},flags{};
 std::array<unsigned,2> dimensions{},offset_bits{};
 unsigned strength_bits{};
 std::array<unsigned,5> named_ids{};
 std::array<std::uintptr_t,5> named_objects{};
 std::array<std::byte,72> packet_bytes{};
 Record draw;HazeBindings bindings;
};
struct HazeCapture {
 std::array<HazeSample,3> samples;HazeSample current;
 unsigned attempts{},count{},reason{},phase{};std::array<unsigned,12> rejected{};
 std::uint64_t last_frame{};bool entered{};
} haze_capture;
constexpr std::array<unsigned,5> haze_ids{0x4beae94,0x4beae90,0x4beae8c,0x4beae84,0x4beae88};
void haze_reject(unsigned reason){haze_capture.reason=reason;++haze_capture.rejected[reason];haze_capture.entered=false;}
bool haze_bindings(std::uintptr_t set,std::uintptr_t graphics,HazeBindings &b,unsigned expected_mask=15,BindingReadProgress *progress=nullptr){
 const auto mark=[&](unsigned phase,unsigned index=0){if(progress)*progress={phase,index};};
 std::uintptr_t ids{},modes{},words{};
 mark(1);
 if(!field(set,0x88,b.count)||!b.count||b.count>64||!field(set,0x80,ids)||!field(graphics,0x10,modes)||!field(set,0,words))return false;
 for(unsigned i=0;i<b.count;++i){mark(2,i);if(!field(ids,4*i,b.ids[i]))return false;
  mark(3,i);if(!field(modes,i,b.modes[i]))return false;mark(4,i);if(!field(graphics,0x30+8*i,b.objects[i]))return false;}
 for(unsigned stage=0;stage<2;++stage){
  const unsigned off=stage?0x14:8;
  mark(5,stage);
  if(!field(set,off,b.ranges[stage])||!field(set,off+4,b.metadata[stage]))return false;
  const auto n=b.ranges[stage][0],start=b.ranges[stage][1];
  mark(6,stage);
  if(n>128||(n&&unsigned(start)+n>4096))return false;
  for(unsigned i=0;i<n;++i){mark(7,stage*128+i);if(!field(words,4*(unsigned(start)+i),b.words[stage][i]))return false;}
 }
 unsigned mask{};
 for(unsigned i=0;i<b.ranges[1][0];++i){const auto word=b.words[1][i],kind=word&15,index=(word>>4)&255,reg=word>>24;
  mark(8,i);
  if(kind>3||(kind&&index>=b.count))return false;
  if(kind!=2)continue;
  mark(8,i);if(reg>=b.textures.size()||(mask&(1U<<reg)))return false;mask|=1U<<reg;b.indices[reg]=index;
  mark(9,reg);if(!tb::read_owner(reader,b.objects[index],b.textures[reg]))return false;
  mark(10,reg);if(!field(b.objects[index],0x78,b.views[reg]))return false;
  mark(11,reg);if(!field(b.views[reg],0x10,b.descriptors[reg])||!b.descriptors[reg])return false;
 }
 mark(12);if(mask!=expected_mask)return false;mark(0);return true;
}
bool haze_before(const pb::Frame &f){
 if(!accepting.load()||f.caller!=cfg.image+0x24c7ff9||f.gpr[1]!=cfg.renderer||f.gpr[2]!=cfg.renderer+0x710)return false;
 Lock lock;auto &c=haze_capture;
 if(c.count==c.samples.size()||c.attempts==16)return false;
 if(c.entered){haze_reject(1);return false;}
 std::uint64_t frame{};if(!field(cfg.renderer,0x1f210,frame)||!frame||frame<=c.last_frame||frame-c.last_frame<60)return false;
 c.last_frame=frame;++c.attempts;c.reason=0;c.phase=1;c.current={};auto &q=c.current;auto &s=q.stamp;
 q.tick=GetTickCount64();s.frame=frame;s.context=f.gpr[2];unsigned char async{};
 if(!field(cfg.renderer,0x93,async)||!field(cfg.renderer,0x1f208,s.epoch)||!field(cfg.renderer,0x1f200,s.bank)||!field(s.context,0x1d0,s.view)){haze_reject(2);return false;}
 s.async=async;if(!same_stamp(s)){haze_reject(2);return false;}
 std::uintptr_t manager{};unsigned count{},matches{};
 if(!field(cfg.image,0x51cc238,manager)||!field(manager,0x3c,count)||!count||count>16){haze_reject(3);return false;}
 for(unsigned i=0;i<count;++i){std::uintptr_t entry{},unit{},type{};unsigned stage{},order{};
  if(!field(manager,0x40+8*i,entry)||!field(entry,0,stage)||!field(entry,4,order)||!field(entry,8,unit)||
     (unit&&!field(unit,0,type))){haze_reject(3);return false;}
  if(stage!=15||order!=20000||type!=cfg.image+0x35372b0)continue;
  std::uintptr_t src{},dst{};
  if(++matches!=1||entry!=unit+0x140||!field(entry,0x10,src)||!field(entry,0x18,dst)||src==dst||!owner(src,q.source)||!owner(dst,q.destination)){haze_reject(3);return false;}
  q.unit=unit;
 }
 if(matches!=1){haze_reject(3);return false;}
 for(unsigned i=0;i<haze_ids.size();++i){
  if(!field(cfg.image,haze_ids[i],q.named_ids[i])||(q.named_ids[i]!=UINT_MAX&&!field(s.context,0x710+8*(q.named_ids[i]&0xfff),q.named_objects[i]))){haze_reject(4);return false;}
 }
 std::array<unsigned char,4> flags{};
 if(q.named_ids[0]==UINT_MAX||q.named_objects[0]!=q.source.object||
    !field(f.gpr[3],0,q.rectangle)||!field(f.gpr[4],0,q.dimensions)||!field(f.stack[0],0,q.offset_bits)||
    !field(q.unit,0x1bc,q.strength_bits)||!field(q.unit,0x1d0,flags)||
    !field(s.context,0x2b78,q.queue)||!q.queue||!field(s.context,0x2b80,q.before)||q.before>=16384||
    q.queue>UINTPTR_MAX-16ULL*(q.before+1)){haze_reject(4);return false;}
 for(unsigned i=0;i<4;++i)q.flags[i]=flags[i];c.entered=true;c.phase=2;return true;
}
void haze_after(bool entered,bool returned){
 if(!entered)return;Lock lock;auto &c=haze_capture;if(!c.entered)return;
 auto &q=c.current;auto &r=q.draw;std::uintptr_t queue{};unsigned key{},opcode{};
 if(!returned||!same_stamp(q.stamp)||!field(q.stamp.context,0x2b78,queue)||queue!=q.queue||
    !field(q.stamp.context,0x2b80,q.after)||q.after!=q.before+1||
    !field(queue+16ULL*q.before,0,key)||key!=0x78004e20||!field(queue+16ULL*q.before,8,q.packet)||
    !field(q.packet,0,opcode)||(opcode&31)!=0||!field(q.packet,0,q.packet_bytes)){haze_reject(5);return;}
 r.draw.packet=q.packet;r.output=q.destination;
 if(!source(r)){haze_reject(6);return;}
 c.phase=3;unsigned flags{},format{};unsigned char rotating{},stride{};unsigned offset{};
 std::uintptr_t depth{},view{},backend{},resource{},arena{};
 if(!field(r.packet_bindings[0],0x48,flags)||(flags&7)!=1||!field(r.packet_bindings[0],0x40,depth)||depth||
    !field(r.packet_bindings[0],0,view)||!field(view,0x60,backend)||!field(backend,0x18,resource)||resource!=q.destination.resource||
    !field(backend,0x20,format)||format!=DXGI_FORMAT_R11G11B10_FLOAT||!field(backend,0x28,rotating)||rotating||
    !field(r.packet_bindings[1],0x20,stride)||stride!=16||!field(r.packet_bindings[1],0x24,offset)||offset>16777216||
    !field(q.stamp.context,0x2b50,arena)||!arena||arena>UINTPTR_MAX-33554432){haze_reject(6);return;}
 r.vertex_floats=r.arguments[0]*4;
 if(!pb::copy_memory(arena+offset+16ULL*r.arguments[1],r.vertices.data(),r.vertex_floats*4)||
    !pb::snapshot_native_pipeline(cfg.image,q.packet,r.before.pipeline)||r.before.pipeline.input_count!=2||
    !field(r.before.pipeline.shader_handle,0,r.draw.shader_set)||!layout(r,false)){haze_reject(6);return;}
 for(unsigned i=0;i<r.vertex_floats;++i)if(!std::isfinite(r.vertices[i])){haze_reject(6);return;}
 c.phase=4;
 if(!shader(r.shaders[0],r.draw.shader_set,0x50,1)||!shader(r.shaders[1],r.draw.shader_set,0x58,0)||
    pb::bytes_sha256(r.shaders[0].bytes.data(),r.shaders[0].size)!="c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6"||
    pb::bytes_sha256(r.shaders[1].bytes.data(),r.shaders[1].size)!="4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69"){haze_reject(7);return;}
 for(unsigned off:{0x60U,0x68U,0x70U}){std::uintptr_t p{};if(!field(r.draw.shader_set,off,p)||p){haze_reject(7);return;}}
 c.phase=5;if(!haze_bindings(r.draw.shader_set,r.packet_bindings[2],q.bindings)){haze_reject(8);return;}
 for(unsigned reg=0;reg<4;++reg){const auto index=q.bindings.indices[reg];bool found=false;
  for(unsigned named=reg;named<=(reg==3?4:reg);++named)if(q.named_ids[named]!=UINT_MAX&&
      (q.named_ids[named]&0xfff)==(q.bindings.ids[index]&0xfff)&&q.named_objects[named]==q.bindings.objects[index])found=true;
  if(!found){haze_reject(9);return;}
 }
 c.phase=6;HazeBindings checked{};pb::NativePipeline pipeline{};std::array<std::byte,72> packet{};
 std::array<float,16> vertices{};tb::Owner src{},dst{};
 if(!same_stamp(q.stamp)||!field(q.packet,0,packet)||packet!=q.packet_bytes||!field(q.stamp.context,0x2b78,queue)||queue!=q.queue||
    !field(q.stamp.context,0x2b80,key)||key!=q.after||
    !pb::snapshot_native_pipeline(cfg.image,q.packet,pipeline)||pipeline!=r.before.pipeline||!layout(r,true)||
    !shader_stable(r.shaders[0],r.draw.shader_set,0x50)||!shader_stable(r.shaders[1],r.draw.shader_set,0x58)||
    !pb::copy_memory(arena+offset+16ULL*r.arguments[1],vertices.data(),r.vertex_floats*4)||vertices!=r.vertices||
    !haze_bindings(r.draw.shader_set,r.packet_bindings[2],checked)||checked!=q.bindings||
    !tb::read_owner(reader,q.source.object,src)||src!=q.source||!tb::read_owner(reader,q.destination.object,dst)||dst!=q.destination){haze_reject(10);return;}
 c.samples[c.count++]=std::move(q);c.entered=false;c.phase=7;c.reason=0;
}
void haze_write(std::ostream &o){
 const auto a=[&](const auto &v){o<<'[';bool comma=false;for(auto x:v){if(comma)o<<',';o<<+x;comma=true;}o<<']';};
 const auto texture=[&](const tb::Owner &v){a(std::array<std::uint64_t,13>{v.object,v.type,v.id,v.kind,v.flags,v.width,v.height,v.format,v.gpu_format,v.primary,v.secondary,v.backing,v.resource});};
 const auto &c=haze_capture;o<<"{\"schema\":2,\"policy\":\"bounded-haze-enqueue-v2\",\"diagnostic_only\":true,\"capacity\":3,\"attempt_limit\":16,\"attempts\":"<<c.attempts
 <<",\"count\":"<<c.count<<",\"entered\":"<<c.entered<<",\"phase\":"<<c.phase<<",\"reason\":"<<c.reason<<",\"rejected\":";a(c.rejected);o<<",\"samples\":[";
 for(unsigned i=0;i<c.count;++i){if(i)o<<',';const auto &q=c.samples[i];const auto &r=q.draw;const auto &b=q.bindings;
  o<<"{\"at\":";a(std::array<std::uint64_t,8>{q.tick,q.stamp.frame,q.stamp.epoch,q.stamp.context,q.stamp.view,q.stamp.bank,q.stamp.async,q.unit});
  o<<",\"queue\":";a(std::array<std::uint64_t,4>{q.queue,q.before,q.after,q.packet});o<<",\"source\":";texture(q.source);o<<",\"destination\":";texture(q.destination);
  o<<",\"named_ids\":";a(q.named_ids);o<<",\"named_objects\":";a(q.named_objects);o<<",\"flags\":";a(q.flags);o<<",\"strength_bits\":"<<q.strength_bits;
  o<<",\"rectangle\":";a(q.rectangle);o<<",\"dimensions\":";a(q.dimensions);o<<",\"offset_bits\":";a(q.offset_bits);
  o<<",\"draw_rectangle\":";a(r.rectangle);o<<",\"arguments\":";a(r.arguments);o<<",\"vertices_bits\":[";
  for(unsigned j=0;j<r.vertex_floats;++j){if(j)o<<',';unsigned bits{};std::memcpy(&bits,&r.vertices[j],4);o<<bits;}o<<']';
  o<<",\"shaders\":[";for(unsigned j=0;j<2;++j){if(j)o<<',';o<<"{\"size\":"<<r.shaders[j].size<<",\"sha256\":"<<std::quoted(pb::bytes_sha256(r.shaders[j].bytes.data(),r.shaders[j].size))<<'}';}o<<']';
  o<<",\"layout\":[";for(unsigned j=0;j<r.before.pipeline.input_count;++j){if(j)o<<',';const auto &e=r.elements[j];o<<"{\"semantic\":"<<std::quoted(r.semantics[j].data())<<",\"fields\":";a(std::array<unsigned,6>{e.SemanticIndex,unsigned(e.Format),e.InputSlot,e.AlignedByteOffset,unsigned(e.InputSlotClass),e.InstanceDataStepRate});o<<'}';}o<<']';
  o<<",\"depth\":";a(r.before.pipeline.depth);o<<",\"blend\":";a(r.before.pipeline.blend);o<<",\"raster\":";a(r.before.pipeline.raster);
  o<<",\"owners\":[";for(unsigned j=0;j<b.count;++j){if(j)o<<',';a(std::array<std::uint64_t,3>{b.ids[j],b.modes[j],b.objects[j]});}o<<']';
  o<<",\"stage_bindings\":[";for(unsigned stage=0;stage<2;++stage){if(stage)o<<',';
   o<<"{\"count\":"<<b.ranges[stage][0]<<",\"start\":"<<b.ranges[stage][1]<<",\"metadata\":";a(b.metadata[stage]);
   o<<",\"words\":";a(std::span(b.words[stage]).first(b.ranges[stage][0]));o<<'}';}o<<']';
  o<<",\"textures\":[";for(unsigned j=0;j<4;++j){if(j)o<<',';o<<"{\"register\":"<<j<<",\"index\":"<<b.indices[j]<<",\"view\":"<<b.views[j]<<",\"cpu_descriptor\":"<<b.descriptors[j]<<",\"owner\":";texture(b.textures[j]);o<<'}';}o<<']';
  o<<",\"same_frame_enqueued\":true,\"gpu_bindings_verified\":false,\"resource_states_verified\":false}";
 }o<<"]}";
}

// Post-chain census (v0.24.10 diagnostic, docs/research/full-size-post-chain.md).
// Included under the route's lock; it reads the game's queue and changes
// nothing. For a sampled frame it records:
// - every descriptor of the post manager: stage, order, type (vtable RVA),
//   input and output owners, in-place flags;
// - every queued packet keyed at stage 15 or later (key >= 0x78000000), with
//   its opcode and size facts: a draw's rectangle and render target, a
//   dispatch's groups, a copy's rectangle, source and destination;
// - the TAA bindings (the seven temporal inputs).
// Each render target and copy endpoint is described once (D3D12 desc).
// Sampled: the first frame of each output/render extent (up to four), and the
// latest every 3,000 frames. Each sample is written once to the summary.
struct PostCensus {
 struct Packet {unsigned key{},opcode{};std::array<int,4> rect{};unsigned target{UINT_MAX},source{UINT_MAX};std::array<unsigned,3> groups{};};
 struct Resource {std::uint64_t pointer{};unsigned width{},height{},format{},flags{};};
 struct Descriptor {unsigned stage{},order{},type{},flags{};std::uint64_t input{},output{};};
 struct Sample {
  std::uint64_t frame{};unsigned render_w{},render_h{},width{},height{};bool printed{true};
  unsigned commands{},descriptor_count{},packet_count{},resource_count{},dropped{},unreadable{};
  std::array<Descriptor,16> descriptors{};std::array<Packet,224> packets{};std::array<Resource,64> resources{};
  std::array<std::array<unsigned,5>,tb::roles> inputs{};  // width, height, format, gpu format, resource index
 };
 std::array<Sample,5> samples{};  // 0-3: first per extent; 4: latest
 std::uint64_t captured{},next_latest{};
};
PostCensus census;
// A pointer that is one of this device's D3D12 resources: its description and
// plane count, with a reference the caller adopts. The SEH boundary holds no
// destructible C++ locals; a failure releases what it acquired.
bool compute_describe(std::uintptr_t candidate,ID3D12Resource **out,D3D12_RESOURCE_DESC *desc,unsigned *planes) noexcept {
 ID3D12Resource *held{};ID3D12Device *device{};bool ok=false;
 __try {
  auto *r=reinterpret_cast<ID3D12Resource *>(candidate);
  if(SUCCEEDED(r->QueryInterface(IID_PPV_ARGS(&held)))&&held==r&&
     SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device)==cfg.device){
   *desc=held->GetDesc();D3D12_FEATURE_DATA_FORMAT_INFO info{desc->Format,0};
   ok=SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,&info,sizeof(info)));
   if(ok)*planes=info.PlaneCount;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
 if(device)device->Release();if(!ok&&held){held->Release();held=nullptr;}*out=held;return ok;
}
unsigned census_resource(PostCensus::Sample &s,std::uintptr_t pointer){
 if(!pointer)return UINT_MAX;
 for(unsigned i=0;i<s.resource_count;++i)if(s.resources[i].pointer==pointer)return i;
 if(s.resource_count==s.resources.size())return UINT_MAX-1;
 auto &r=s.resources[s.resource_count];r={pointer};
 ID3D12Resource *held{};D3D12_RESOURCE_DESC desc{};unsigned planes{};
 if(compute_describe(pointer,&held,&desc,&planes)){
  r.width=static_cast<unsigned>(desc.Width);r.height=desc.Height;r.format=static_cast<unsigned>(desc.Format);r.flags=static_cast<unsigned>(desc.Flags);held->Release();}
 return s.resource_count++;
}
// An engine owner's D3D12 resource (the copy helper's owners).
std::uintptr_t census_owner_resource(std::uintptr_t object){
 tb::Owner o;return object&&tb::read_owner(reader,object,o)?o.resource:0;
}
void census_capture(PostCensus::Sample &s,std::uint64_t frame,std::uintptr_t context,const tb::Snapshot *stamp){
 s=PostCensus::Sample{};s.frame=frame;s.render_w=render_w;s.render_h=render_h;s.width=width;s.height=height;s.printed=false;
 std::uintptr_t filters{};unsigned count{};
 if(field(cfg.image,0x51cc238,filters)&&field(filters,0x3c,count)&&count<=16)
  for(unsigned i=0;i<count;++i){std::uintptr_t e{},u{},type{};auto &d=s.descriptors[s.descriptor_count];std::array<unsigned char,3> flags{};
   if(!field(filters,0x40+8*i,e)||!field(e,0,d.stage)||!field(e,4,d.order)||!field(e,8,u)||!field(e,0x10,d.input)||!field(e,0x18,d.output)||
      !field(e,0x20,flags)||(u&&!field(u,0,type)))break;
   d.type=type>=cfg.image&&type-cfg.image<0x10000000?static_cast<unsigned>(type-cfg.image):0;
   d.flags=flags[0]|(flags[1]<<8)|(flags[2]<<16);++s.descriptor_count;}
 if(stamp)for(unsigned i=0;i<tb::roles;++i){const auto &o=stamp->inputs[i];
  s.inputs[i]={o.width,o.height,o.format,o.gpu_format,census_resource(s,o.resource)};}
 std::uintptr_t array{};unsigned commands{};
 if(!field(context,0x2b80,commands)||!commands||commands>=65535||!field(context,0x2b78,array)||!array)return;
 s.commands=commands;
 struct Command {unsigned key{},padding{};std::uintptr_t packet{};};static_assert(sizeof(Command)==16);
 std::array<Command,256> block;
 for(unsigned base=0;base<commands;base+=256){const auto n=(std::min)(256U,commands-base);
  if(!pb::copy_memory(array+16ULL*base,block.data(),n*sizeof(Command))){++s.unreadable;return;}
  for(unsigned j=0;j<n;++j){const auto &c=block[j];if(c.key<0x78000000U)continue;
   if(s.packet_count==s.packets.size()){++s.dropped;continue;}
   auto &k=s.packets[s.packet_count];k=PostCensus::Packet{};k.key=c.key;unsigned raw{};
   if(!field(c.packet,0,raw)){k.opcode=UINT_MAX;++s.packet_count;continue;}
   k.opcode=raw&31;
   if(k.opcode==0){std::uintptr_t rect{},view{},backend{},resource{};
    if(field(c.packet,0x10,rect)&&field(rect,0x4c,k.rect)&&field(rect,0,view)&&field(view,0x60,backend)&&field(backend,0x18,resource))
     k.target=census_resource(s,resource);
   }else if(k.opcode==6){(void)(field(c.packet,0x18,k.groups));
   }else if(k.opcode==13){std::uintptr_t dst{},src{};std::array<unsigned,4> rect{};
    if(field(c.packet,0x10,dst)&&field(c.packet,0x18,src)&&field(c.packet,0x38,rect)){
     for(unsigned i=0;i<4;++i)k.rect[i]=static_cast<int>(rect[i]);
     k.target=census_resource(s,census_owner_resource(dst));k.source=census_resource(s,census_owner_resource(src));}
   }
   ++s.packet_count;
  }
 }
}
void census_observe(std::uint64_t frame,std::uintptr_t context,const tb::Snapshot *stamp){
 unsigned slot=UINT_MAX;bool known=false;
 for(unsigned i=0;i<4;++i){const auto &s=census.samples[i];
  if(s.frame&&s.render_w==render_w&&s.render_h==render_h&&s.width==width&&s.height==height)known=true;}
 if(!known)for(unsigned i=0;i<4;++i)if(!census.samples[i].frame){slot=i;break;}
 if(slot==UINT_MAX&&frame>=census.next_latest){slot=4;census.next_latest=frame+3000;}
 if(slot==UINT_MAX)return;
 census_capture(census.samples[slot],frame,context,stamp);++census.captured;
}
void census_write(std::ostream &o){
 o<<"{\"captured\":"<<census.captured<<",\"samples\":[";bool first=true;
 for(auto &s:census.samples){if(s.printed||!s.frame)continue;s.printed=true;if(!first)o<<',';first=false;
  o<<"{\"frame\":"<<s.frame<<",\"render\":["<<s.render_w<<','<<s.render_h<<"],\"output\":["<<s.width<<','<<s.height
   <<"],\"commands\":"<<s.commands<<",\"dropped\":"<<s.dropped<<",\"unreadable\":"<<s.unreadable<<",\"descriptors\":[";
  for(unsigned i=0;i<s.descriptor_count;++i){const auto &d=s.descriptors[i];if(i)o<<',';
   o<<'['<<d.stage<<','<<d.order<<','<<d.type<<','<<d.input<<','<<d.output<<','<<d.flags<<']';}
  o<<"],\"inputs\":[";
  for(unsigned i=0;i<s.inputs.size();++i){const auto &v=s.inputs[i];if(i)o<<',';o<<'['<<v[0]<<','<<v[1]<<','<<v[2]<<','<<v[3]<<','<<static_cast<int>(v[4])<<']';}
  o<<"],\"resources\":[";
  for(unsigned i=0;i<s.resource_count;++i){const auto &r=s.resources[i];if(i)o<<',';
   o<<'['<<r.pointer<<','<<r.width<<','<<r.height<<','<<r.format<<','<<r.flags<<']';}
  o<<"],\"packets\":[";
  for(unsigned i=0;i<s.packet_count;++i){const auto &k=s.packets[i];if(i)o<<',';
   o<<'['<<k.key<<','<<static_cast<int>(k.opcode)<<','<<k.rect[0]<<','<<k.rect[1]<<','<<k.rect[2]<<','<<k.rect[3]<<','
    <<static_cast<int>(k.target)<<','<<static_cast<int>(k.source)<<','<<k.groups[0]<<','<<k.groups[1]<<','<<k.groups[2]<<']';}
  o<<"]}";
 }
 o<<"]}";
}

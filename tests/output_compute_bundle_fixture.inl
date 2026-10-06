// Complete observed Gallery opcode sequence with synthetic compute recipes.
// Synthetic shaders/CPU constants; private game assets are never embedded.
{
 auto cg=g;cg.command_gate={7,0x78006595,1,1,6,1,0,0};cg.filters[0][2]=26005;cg.filters[0][5]=objects[1];
 const auto native_filter=std::array<std::uint64_t,4>{15ULL|(26005ULL<<32),0x9000,objects[1],objects[0]};m.put(filter,native_filter);
 const auto cq=m.take(16*17),state_packet=m.take(16);m.put(context+0x2b78,cq);m.put(context+0x2b80,8U);
 m.put(cq,0x78006595U);m.put(cq+8,state_packet);m.put(state_packet,11U);
 for(auto object:objects){const auto srv=m.take(32),uav=m.take(32);m.put(object+0x78,srv);m.put(object+0x80,uav);m.put(srv+0x10,srv+16);m.put(uav+0x10,uav+16);}
 constexpr std::array<std::array<unsigned,3>,6> groups{{{8,1440,1},{128,5,1},{16,9,1},{80,45,1},{80,45,1},{80,45,1}}};
 std::array<std::uint64_t,6> cssets{},packets{},descriptors{},bindings{};
 const std::array<std::uint64_t,3> cb{m.take(1072),m.take(112),m.take(96)};
 m.put(cb[0]+416,123.F);m.put(cb[1]+16,2560.F);m.put(cb[1]+40,1280U);m.put(cb[2],.25F);
 for(unsigned i=0;i<6;++i){const auto p=m.take(48),state=m.take(0x80),handle=m.take(8),set=m.take(0x90),ids=m.take(24),modes=m.take(6),words=m.take(24),se=m.take(0x20),sd=m.take(16),data=m.take(52);
  packets[i]=p;cssets[i]=set;descriptors[i]=sd;bindings[i]=words;
  m.put(cq+16*(i+1),0x78006595U);m.put(cq+16*(i+1)+8,p);m.put(p,0xa0000006U);m.put(p+0x10,state);m.put(p+0x18,groups[i]);m.put(p+0x24,0xa55a0100U);
  m.put(state+8,handle);m.put(handle,set);m.put(state+0x10,modes);m.put(state+0x18,100U+i);m.put(set+0x80,ids);m.put(set+0x88,6U);
  m.put(ids,std::array<unsigned,6>{1,2,3,11,12,501});m.put(state+0x30,objects[i%2]);m.put(state+0x38,objects[(i+1)%2]);m.put(state+0x40,std::uint64_t{0x500});
  for(unsigned cbi=0;cbi<3;++cbi)m.put(state+0x48+8*cbi,cb[cbi]);
  m.put(set,words);m.put(set+0x44,std::array<unsigned short,2>{6,0});m.put(words,std::array<unsigned,6>{2,19,33,0xb030,0x100c040,0x31f5050});
  m.put(set+0x78,se);m.put(se+0x10,sd);m.put(sd,52U);m.put(sd+8,data);
  m.put(data,0x43425844U);m.put(data+20,1U);m.put(data+24,52U);m.put(data+28,1U);m.put(data+32,36U);
  m.put(data+36,0x52444853U);m.put(data+40,8U);m.put(data+44,0x50050U);m.put(data+48,2U);
 }
 m.put(cq+16*7,0x78006595U);m.put(cq+16*7+8,s.draws[1].packet);
 const auto graphics=std::uint64_t(s.draws[1].prefix[8])|(std::uint64_t(s.draws[1].prefix[9])<<32);
 std::uint64_t draw_ids{},draw_words{},first_state{},first_modes{};
 check(m.read(sets[1]+0x80,&draw_ids,8)&&m.read(sets[1],&draw_words,8)&&m.read(packets[0]+0x10,&first_state,8)&&m.read(first_state+0x10,&first_modes,8),"fixture state");
 m.put(sets[1]+0x88,2U);m.put(draw_ids+4,12U);m.put(graphics+0x38,cb[1]);
 m.put(sets[1]+0x14,std::array<unsigned short,2>{2,0});m.put(draw_words+4,0x100c010U);
 const auto observe=[&](Capture &t){t.observe_compute_bundle(cg,read,[]{return true;},0x9100);};
 auto bundle=std::make_unique<Capture>(true);observe(*bundle);
 check(bundle->size()==1,"complete compute bundle captured");const auto &bs=bundle->sample(0);
 check(bs.command_count==8&&bs.compute_count==6&&bs.count==1,"all eight native commands accounted");
 check(bs.computes[0].prefix[6]==8&&bs.computes[5].prefix[7]==45&&bs.commands[7][3]==0,"dispatch dimensions and draw index");
 check(bs.computes[0].constant_count==3&&bs.computes[0].constants[1].size==112,"typed constant sizes");
 check(bs.draws[0].constant_count==1&&bs.draws[0].constants[0].word==0x100c010,"composition constants captured independently");
 check(std::memcmp(bs.computes[0].constants[1].bytes.data(),m.bytes.data()+cb[1],112)==0,"screen constants copied");
 check(bs.computes[0].textures[1].fields[3]==bs.computes[1].textures[0].fields[3],"compute UAV to SRV edge");emit("compute-bundle",*bundle);
 auto scratch=std::make_unique<Capture::Compute>();
 check(Capture::compute_matches(bs.computes[0],*scratch,read),"native recording recipe remains identical");
 m.put(cb[1]+16,1920.F);check(!Capture::compute_matches(bs.computes[0],*scratch,read),"changed recording constants rejected");
 check(Capture::compute_reread(bs.computes[0],*scratch,read)&&Capture::compute_difference(bs.computes[0],*scratch)==128,"constant change isolated from unreadable recipe");m.put(cb[1]+16,2560.F);
 m.put(packets[0]+0x24,0xb55a0100U);
 check(Capture::compute_reread(bs.computes[0],*scratch,read)&&Capture::compute_difference(bs.computes[0],*scratch)==2,"adjacent mode bytes identified without weakening comparison");m.put(packets[0]+0x24,0xa55a0100U);
 m.put(packets[0]+0x28,std::uint64_t{1});check(!Capture::compute_reread(bs.computes[0],*scratch,read),"unsupported packet remains unreadable comparison");m.put(packets[0]+0x28,std::uint64_t{});
 check(Capture::compute_matches(bs.computes[0],*scratch,read)&&!Capture::compute_difference(bs.computes[0],*scratch),"original comparison restored");
 // Resource-state planning may install, replace, or clear the +8 list head
 // after capture. That must not hide any stable recipe change or relax the
 // exact double snapshot used while the queue is captured.
 for(const auto head:{std::uint64_t{0x14a35580},std::uint64_t{0x12345678abcdef00},std::uint64_t{0}}){
  m.put(packets[0]+8,head);
  check(Capture::compute_reread(bs.computes[0],*scratch,read),"scheduled recipe readable");
  check(!Capture::compute_recording_difference(bs.computes[0],*scratch),"state-list scheduling change is not a recipe change");
  check(Capture::compute_matches(bs.computes[0],*scratch,read)==(head==0),"same-stage equality still includes both pointer words");
 }
 for(unsigned word=0;word<12;++word){
  *scratch=bs.computes[0];scratch->prefix[2]=0x14a35580;scratch->prefix[3]=0x12;
  scratch->prefix[word]^=0x80000000U;
  check(bool(Capture::compute_recording_difference(bs.computes[0],*scratch))==(word!=2&&word!=3),"all other packet words remain protected alongside pointer change");
 }
 const auto reject_recipe_change=[&](auto change){
  *scratch=bs.computes[0];scratch->prefix[2]=0x14a35580;scratch->prefix[3]=0x12;change(*scratch);
  check(Capture::compute_recording_difference(bs.computes[0],*scratch)!=0,"changed stable recipe cannot hide behind scheduled pointer");
 };
 reject_recipe_change([](auto &c){++c.packet;});reject_recipe_change([](auto &c){++c.index;});
 reject_recipe_change([](auto &c){++c.shader_set;});reject_recipe_change([](auto &c){++c.technique;});
 reject_recipe_change([](auto &c){++c.owner_count;});reject_recipe_change([](auto &c){++c.objects[0];});
 reject_recipe_change([](auto &c){++c.ids[0];});reject_recipe_change([](auto &c){++c.modes[0];});
 reject_recipe_change([](auto &c){++c.textures[0].fields[3];});reject_recipe_change([](auto &c){c.texture_used[0]=!c.texture_used[0];});
 reject_recipe_change([](auto &c){++c.bindings.range[0];});reject_recipe_change([](auto &c){++c.bindings.words[0];});
 reject_recipe_change([](auto &c){++c.views[0][0];});reject_recipe_change([](auto &c){++c.views[0][1];});
 reject_recipe_change([](auto &c){++c.shader.identity[0];});reject_recipe_change([](auto &c){++c.shader.size;});
 reject_recipe_change([](auto &c){++c.shader.bytes[0];});reject_recipe_change([](auto &c){++c.constant_count;});
 reject_recipe_change([](auto &c){++c.constants[0].word;});reject_recipe_change([](auto &c){++c.constants[0].size;});
 reject_recipe_change([](auto &c){++c.constants[0].bytes[0];});
 auto changing_schedule=std::make_unique<Capture>(true);unsigned prefix_reads{};
 changing_schedule->observe_compute_bundle(cg,[&](std::uint64_t at,void *out,std::size_t n){
  if(at==packets[0]&&n==48&&++prefix_reads==2)m.put(packets[0]+8,std::uint64_t{0x14a35580});
  return read(at,out,n);},[]{return true;},0x9100);
 check(!changing_schedule->size(),"changing schedule during same-stage double snapshot still rejected");
 m.put(packets[0]+8,std::uint64_t{});
 auto ignored=std::make_unique<Capture>(true);auto reads_before=m.reads;const auto valid=cg;
 for(unsigned i=0;i<5;++i){cg=valid;if(i==0)cg.reason=6;if(i==1)cg.command_gate[0]=9;if(i==2)cg.command_gate[4]=11;if(i==3)cg.filters[0][4]++;if(i==4){cg.count=2;cg.filters[1]=cg.filters[0];}observe(*ignored);}
 check(!ignored->attempts()&&reads_before==m.reads,"unrelated compute rejection ignored");cg=valid;
 const auto invalid=[&](const char *name){auto b=std::make_unique<Capture>(true);observe(*b);check(b->attempts()==1&&!b->size(),name);emit(name,*b);};
 m.put(packets[0]+0x24,0xa55a0101U);invalid("compute-indirect-mode");m.put(packets[0]+0x24,0xa55a0100U);
 m.put(packets[0]+0x28,std::uint64_t{1});invalid("compute-indirect-pointer");m.put(packets[0]+0x28,std::uint64_t{});
 m.put(bindings[0],1026U);invalid("compute-owner-index");m.put(bindings[0],2U);
 m.put(cssets[0]+0x44,std::array<unsigned short,2>{129,0});invalid("compute-range-bound");m.put(cssets[0]+0x44,std::array<unsigned short,2>{6,0});
 m.put(bindings[0]+12,0xb400U);invalid("constant-owner-bound");m.put(bindings[0]+12,0xb030U);
 m.put(bindings[0]+12,0xc030U);invalid("constant-unexpected-buffer");m.put(bindings[0]+12,0xb030U);
 m.put(first_modes+3,static_cast<unsigned char>(1));invalid("constant-owner-mode");m.put(first_modes+3,static_cast<unsigned char>(0));
 m.put(first_state+0x48,std::uint64_t{0x100000});invalid("constant-unreadable-owner");m.put(first_state+0x48,cb[0]);
 m.put(first_state+0x48,cb[0]+1);invalid("constant-owner-alignment");m.put(first_state+0x48,cb[0]);
 auto unstable_cb=std::make_unique<Capture>(true);unsigned cb_reads{};
 unstable_cb->observe_compute_bundle(cg,[&](std::uint64_t at,void *out,std::size_t n){if(at==cb[1]&&++cb_reads==7)m.put(cb[1]+16,1920.F);return read(at,out,n);},[]{return true;},0x9100);
 check(!unstable_cb->size(),"changing screen constants rejected");emit("constant-changing",*unstable_cb);m.put(cb[1]+16,2560.F);
 m.put(descriptors[0],65537U);invalid("compute-shader-bound");m.put(descriptors[0],52U);
 m.put(filter+16,objects[0]);invalid("compute-stale-filter");m.put(filter,native_filter);
 m.put(packets[2],13U);invalid("compute-unexpected-command");m.put(packets[2],0xa0000006U);
 auto changed=std::make_unique<Capture>(true);unsigned scan{};
 changed->observe_compute_bundle(cg,[&](std::uint64_t at,void *out,std::size_t n){if(at==cq&&++scan==2)m.put(packets[4]+0x18,81U);return read(at,out,n);},[]{return true;},0x9100);
 check(!changed->size(),"changed dispatch rejected");emit("compute-changing",*changed);m.put(packets[4]+0x18,80U);
 for(unsigned i=8;i<17;++i){m.put(cq+16*i,0x78006595U);m.put(cq+16*i+8,state_packet);}
 m.put(context+0x2b80,17U);invalid("compute-command-bound");m.put(context+0x2b80,8U);
 before=m.reads;observe(*bundle);check(before==m.reads,"compute sample throttled");cg.at[1]+=60;observe(*bundle);check(bundle->size()==2,"compute second sample");emit("compute-full",*bundle);
 cg.at[1]+=60;before=m.reads;observe(*bundle);check(before==m.reads,"compute capacity stops all reads");
 m.put(context+0x2b78,queue);m.put(context+0x2b80,4U);m.put(filter,std::array<std::uint64_t,4>{15ULL|(4100ULL<<32),0x9000,0,objects[0]});
 m.put(sets[1]+0x88,1U);m.put(sets[1]+0x14,std::array<unsigned short,2>{1,0});
}

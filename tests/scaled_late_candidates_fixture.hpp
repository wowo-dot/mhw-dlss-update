#pragma once
// The late indexed scan (hunter silhouette pieces) on a synthetic completed
// queue. Up to Plan::late_capacity packets are frozen in native key order
// (stable for equal keys); more leave the frame native with reason 2 (game-45:
// Alatreon's arrival exceeded the former 32), a repeated packet with reason 3.
void check_late_candidates(fx::Fixture &f){
 using stream_plan_fixture::Arena;using stream_plan_fixture::put;using fx::require;
 Arena image(0x5200000),a(0x80000);const auto renderer=a.take(0x21000),context=renderer+0x710,view=a.take(0x100);
 const auto saved=route::cfg;route::cfg={image.base,renderer,0,reinterpret_cast<std::uintptr_t>(f.device.Get())};
 auto storage=std::make_unique<route::Plan>();auto &p=*storage;auto &s=p.stamp;
 s.context=context;s.view=view;s.frame=9;s.epoch=9;s.async=1;
 put(renderer+0x93,0xa55a0101U);put(renderer+0x1f210,s.frame);put(renderer+0x1f208,s.epoch);put(renderer+0x1f450,view);put(context+0x1d0,view);
 constexpr unsigned most=80;const auto queue=a.take(16*(most+4));std::array<std::uintptr_t,most> objects{};
 for(auto &o:objects){o=a.take(0x50);put(o,1U);put(o+0x40,std::array<unsigned,3>{6,1,0});}
 const auto state=a.take(0x50);put(state,0U);
 // A copy before the endpoint, a late state packet and the endpoint key are
 // never candidates. Late keys repeat (three values) to prove stable order.
 const auto fill=[&](unsigned late,bool duplicate){
  unsigned n{};const auto entry=[&](unsigned key,std::uintptr_t object){put(queue+16*n,key);put(queue+16*n+8,object);++n;};
  entry(0x78000002U,state);entry(0x7ffffff1U,objects[0]);entry(0x7ffffff3U,state);
  for(unsigned i=0;i<late;++i)entry(0x7ffffff4U-i%3,duplicate&&i==late-1?objects[0]:objects[i]);
  put(context+0x2b78,queue);put(context+0x2b80,n);};
 const auto frozen=[&](unsigned late){
  if(p.late_count!=late)return false;
  for(unsigned i=1;i<late;++i)if(p.late_keys[i-1]>p.late_keys[i])return false;
  // Within one key the insertion order (object index) is preserved.
  for(unsigned i=1;i<late;++i)if(p.late_keys[i-1]==p.late_keys[i]){
   unsigned a0{},a1{};for(unsigned j=0;j<most;++j){if(objects[j]==p.late_packets[i-1])a0=j;if(objects[j]==p.late_packets[i])a1=j;}
   if(a0>=a1)return false;}
  return true;};
 const auto reasons=route::indexed_candidate_reasons;const auto rejected=route::indexed_candidates_rejected;
 fill(17,false);require(route::silhouette_candidates(p)&&frozen(17),"typical combat frame (17 pieces)");
 fill(40,false);require(route::silhouette_candidates(p)&&frozen(40),"Alatreon arrival above the former 32");
 fill(route::Plan::late_capacity,false);require(route::silhouette_candidates(p)&&frozen(route::Plan::late_capacity),"exact capacity");
 fill(route::Plan::late_capacity+1,false);
 require(!route::silhouette_candidates(p)&&!p.late_count&&route::indexed_candidate_reasons[2]==reasons[2]+1&&
         route::indexed_candidates_max==route::Plan::late_capacity+1,"over capacity stays native with reason 2 and the true count");
 fill(5,true);require(!route::silhouette_candidates(p)&&route::indexed_candidate_reasons[3]==reasons[3]+1,"repeated packet stays native with reason 3");
 put(renderer+0x1f210,s.frame+1);fill(5,false);
 require(!route::silhouette_candidates(p)&&route::indexed_candidate_reasons[5]==reasons[5]+1,"changed frame stays native with reason 5");
 require(route::indexed_candidates_rejected==rejected+3,"three rejected scans");
 // v0.24.1: a late draw whose engine pipeline writes no colour, depth or
 // stencil is classified before any shader lookup (stays native, rejection 11
 // at record) and is not reported as an unknown program; a colour write is not.
 {const auto engine_pipeline=[&](std::uintptr_t o,unsigned char mask){
   const auto graphics=a.take(0x40),render=a.take(0x40),blend_owner=a.take(0xa0),blend=a.take(0x10+sizeof(D3D12_BLEND_DESC)),
              raster_owner=a.take(0x80),raster=a.take(0x10+sizeof(D3D12_RASTERIZER_DESC));
   put(o+0x20,graphics);put(o+0x28,render);put(render+4,static_cast<unsigned char>(0));
   put(render+8,blend_owner);put(render+0x18,raster_owner);put(blend_owner+0x98,blend);put(raster_owner+0x68,raster);
   D3D12_BLEND_DESC b{};b.RenderTarget[0].RenderTargetWriteMask=mask;put(blend+0x10,b);
   put(raster+0x10,D3D12_RASTERIZER_DESC{});};
  // The depth state comes from the renderer's current depth owner: depth and
  // stencil off.
  const auto owners=a.take(0x2490),owner=a.take(0x10),depth_owner=a.take(0x70),depth=a.take(0x10+sizeof(D3D12_DEPTH_STENCIL_DESC));
  put(image.base+0x51c73d0,owners);put(owners+0x2480,owner);put(owner,depth_owner);put(depth_owner+0x60,depth);
  put(depth+0x10,D3D12_DEPTH_STENCIL_DESC{});
  engine_pipeline(objects[1],0);engine_pipeline(objects[2],15);put(renderer+0x1f210,s.frame);
  const auto unknown=route::silhouette_unknown_draws,failed=route::silhouette_recipe_failed;
  fill(5,false);require(route::silhouette_candidates(p)&&frozen(5),"five pieces with engine pipelines");
  unsigned silent{},written{};
  for(unsigned i=0;i<p.late_count;++i){if(p.late_packets[i]==objects[1])silent+=p.late[i].silent&&!p.late[i].recipe;
   if(p.late_packets[i]==objects[2])written+=!p.late[i].silent;}
  require(silent==1&&written==1,"a draw that writes nothing is classified; a colour write is not");
  require(route::silhouette_unknown_draws==unknown&&route::silhouette_recipe_failed>failed,"no unknown program; the other pieces fail lookup");
  for(const auto o:{objects[1],objects[2]}){put(o+0x20,std::uintptr_t{});put(o+0x28,std::uintptr_t{});}}
 std::ostringstream summary;route::write_summary(summary);
 require(summary.str().find("\"late_draws\":{\"planned\":")!=std::string::npos&&summary.str().find("\"max\":65,\"capacity\":64}")!=std::string::npos,"summary reports late draws");
 route::cfg=saved;
 std::cout<<"PASS late indexed scan: 17/40/64 frozen in stable key order; 65, a repeated packet and a changed frame stay native with reasons\n";
}

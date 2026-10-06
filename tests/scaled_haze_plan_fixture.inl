// Continue from the observational fixture with a complete four-effect graph.
  // Restore the ordinary motion packet; give haze its own packet and output.
  put(packets[0]+0x20,old_graphics);put(queue+16,0x78000064U);
  std::vector<ComPtr<ID3D12Resource>> extras;
  const auto make_owner=[&](D3D12_RESOURCE_DESC d,unsigned format,unsigned kind){
   extras.push_back(resource(f.device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON));
   const auto object=a.take(0x100),backend=a.take(0x40),srv=a.take(0x20);
   put(object,image.base+0x3537870);put(object+0x58,kind);put(object+0x60,backend);
   put(object+0x78,srv);put(srv+0x10,std::uintptr_t{200+extras.size()});
   put(object+0x88,static_cast<unsigned>(d.Width)+(d.Height<<16));put(object+0x90,format);
   put(backend+0x20,reinterpret_cast<std::uintptr_t>(extras.back().Get()));return object;};
  const auto output=make_owner(desc,34,5);
  const auto hd=make_owner(texture(128,64,DXGI_FORMAT_R32_FLOAT),16,5);
  auto volume=texture(32,32,DXGI_FORMAT_R8G8B8A8_UNORM);volume.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;volume.DepthOrArraySize=32;
  const auto hv=make_owner(volume,7,1),hf=make_owner(texture(1,1,DXGI_FORMAT_R8G8B8A8_UNORM),7,0);
  const auto color_srv=a.take(0x20);put(owners[2]+0x78,color_srv);put(color_srv+0x10,std::uintptr_t{199});
  const auto true_ids=a.take(64),true_modes=a.take(16);put(set+0x80,true_ids);put(set+0x88,9U);put(graphics+0x10,true_modes);
  constexpr std::array<unsigned,9> captured_words{1060865,17440785,790562,17059890,35762242,52531282,45152,16826480,52543616};
  put(set+0x14,static_cast<unsigned short>(9));put(words,captured_words);
  const std::array<std::uintptr_t,9> true_objects{owners[0],owners[1],owners[2],hd,hv,hf,owners[0],owners[1],owners[2]};
  for(unsigned i=0;i<9;++i){put(true_ids+4*i,100+i);put(graphics+0x30+8*i,true_objects[i]);}
  const auto hp=a.take(0x50),hia=a.take(0x50),hrc=a.take(0x60),hview=a.take(0x80),hvbackend=a.take(0x30),hvertices=a.take(64);
  std::memcpy(reinterpret_cast<void *>(hp),reinterpret_cast<void *>(packets[0]),0x50);
  put(hp+0x10,hrc);put(hp+0x18,hia);put(hp+0x20,graphics);put(hp+0x40,3U);put(hp+0x44,0U);
  put(hia+0x20,static_cast<unsigned char>(16));put(hia+0x24,static_cast<unsigned>(hvertices-vertices));
  put(hvertices,std::array<float,12>{-1,1,0,0,-1,-3,0,1,3,1,1,0});
  put(hrc,hview);put(hrc+0x48,1U);put(hrc+0x4c,std::array<int,4>{0,0,64,32});
  put(hview+0x60,hvbackend);put(hvbackend+0x18,reinterpret_cast<std::uintptr_t>(extras[0].Get()));put(hvbackend+0x20,26U);
  const auto hqueue=a.take(16*6);std::memcpy(reinterpret_cast<void *>(hqueue),reinterpret_cast<void *>(queue),16*4);
  put(hqueue+64,0x78004e20U);put(hqueue+72,hp);put(context+0x2b78,hqueue);put(context+0x2b80,5U);
  put(haze+0x150,owners[2]);put(haze+0x158,output);put(grade+0x248,output);
  auto good_final=captured;good_final.stamp=s;route::final_quads[s.frame%32]=good_final;
  auto admitted_storage=std::make_unique<route::Plan>();auto &admitted=*admitted_storage;admitted.stamp=s;
  require(route::build_plan(admitted,motion)&&admitted.has_haze,"four-effect haze graph admitted");
  require(route::effect_count(admitted)==4&&route::effect_stage(admitted,1)==3&&route::effect_ordinal(admitted,2)==3,"haze execution order");
  require(admitted.resources[5]==extras[0]&&admitted.resources[6]==extras[1]&&admitted.resources[7]==extras[2]&&admitted.resources[8]==extras[3],"actual auxiliary resources pinned");
  auto haze_recipe=route::recipe(admitted.draws[3],3,f.root.Get());sf::Effect haze_effect(f.device.Get(),haze_recipe,true);
  unsigned failures{};
  const auto reject_graph=[&]{auto q_storage=std::make_unique<route::Plan>();auto &q=*q_storage;q.stamp=s;require(!route::build_plan(q,motion),"invalid haze graph refused");++failures;};
  put(haze+0x150,owners[1]);reject_graph();put(haze+0x150,owners[2]);
  put(grade+0x248,owners[2]);reject_graph();put(grade+0x248,output);
  put(context+0x2b80,4U);reject_graph();put(context+0x2b80,5U);
  put(hqueue+80,0x78004e20U);put(hqueue+88,hp);put(context+0x2b80,6U);reject_graph();put(context+0x2b80,5U);
  put(graphics+0x40,owners[1]);reject_graph();put(graphics+0x40,owners[2]);
  put(true_modes+2,static_cast<unsigned char>(1));reject_graph();put(true_modes+2,static_cast<unsigned char>(0));
  put(words+8,790562U^0x1000000U);reject_graph();put(words+8,790562U);
  put(hd+0x90,7U);reject_graph();put(hd+0x90,16U);
  put(hvertices+40,2.F);reject_graph();put(hvertices+40,1.F);
  put(hia+0x20,static_cast<unsigned char>(8));reject_graph();put(hia+0x20,static_cast<unsigned char>(16));
  require(route::haze_bindings_stable(admitted),"haze binding snapshot stable");
  put(graphics+0x40,owners[1]);require(!route::haze_bindings_stable(admitted),"binding mutation after admission rejected");put(graphics+0x40,owners[2]);
  auto restored_storage=std::make_unique<route::Plan>();auto &restored=*restored_storage;restored.stamp=s;require(route::build_plan(restored,motion),"restored complete haze graph");
  check_queue_completion(s,filters,motion,5);
  // The actual sauna alternates two color owners: haze reuses the copy target.
  put(haze+0x158,owners[1]);put(grade+0x248,owners[1]);
  put(hvbackend+0x18,reinterpret_cast<std::uintptr_t>(resources[1].Get()));
  auto alias=std::make_unique<route::Plan>();alias->stamp=s;require(route::build_plan(*alias,motion)&&alias->resources[1]==alias->resources[5],"native haze ping-pong owner reuse");
  check_queue_completion(s,filters,motion,5);
  std::cout<<"PASS haze route graph: "<<failures<<" malformed cases, actual resource pins, immutable binding checks and native PSO\n";


// Actual resource formats, synthetic pixels and explicit fixture descriptors.
// Measure equivalence; preserve a mismatch instead of relaxing its threshold.
void native_format_oracles(const Result &r,const Captured &c){
 const auto w=c.width,h=c.height;const auto blur=c.cb.data()+384;
 for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w/20;++x){
  const auto d=captured_depth(20*x,y,c.view_width,c.view_height,false);
  const std::array<float,4> expected{c.cb[106]/std::fma(-d,c.cb[105],c.cb[104]),std::abs(std::fma(blur[1],d,blur[0])),std::abs(std::fma(blur[1],d,blur[0])),1};
  const auto at=(size_t(y)*(w/20)+x)*4;
  for(unsigned k=0;k<3;++k)if(!dof_formats::brackets(r.stages[0][at+k],expected[k],k)){
   std::cerr<<"horizontal at "<<x<<','<<y<<" channel "<<k<<" depth "<<d<<" actual "<<r.stages[0][at+k]<<" expected "<<expected[k]<<'\n';
   require(false,"native-format horizontal CPU oracle");}
 }
 for(unsigned y=0;y<h/20;++y)for(unsigned x=0;x<w/20;++x){
  const auto src=(size_t(20*y)*(w/20)+x)*4,dst=(size_t(y)*(w/20)+x)*4;
  for(unsigned k=0;k<3;++k)require(r.stages[0][src+k]==r.stages[1][dst+k],"native-format vertical CPU oracle");
 }
 for(unsigned y=0;y<h/2;++y)for(unsigned x=0;x<w/2;++x){
  const unsigned sx=(std::min)(2*x,c.view_width-2),sy=(std::min)(2*y,c.view_height-2);
  std::array<float,4> expected{};
  for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){std::array<float,4> proxy{};
   for(unsigned yy=0;yy<2;++yy)for(unsigned xx=0;xx<2;++xx){const auto v=scene(2*(sx+dx)+xx,2*(sy+dy)+yy);for(unsigned k=0;k<3;++k)proxy[k]+=.25F*v[k];}
   const auto q=dof_formats::quantize(proxy);for(unsigned k=0;k<3;++k)expected[k]+=.25F*q[k];
  }
  for(unsigned k=0;k<3;++k)require(dof_formats::brackets(r.stages[4][(size_t(y)*(w/2)+x)*4+k],expected[k],k),"native-format preparation CPU oracle");
 }
}
void native_format_cases(Pipeline &p,const Captured &c,const std::filesystem::path &out){
 for(unsigned k=0;k<3;++k){
  require(dof_formats::brackets(.5F,.5F,k)&&!dof_formats::brackets(.75F,.5F,k),"packed oracle negative control");
  require(!dof_formats::brackets(-.5F,.5F,k)&&!dof_formats::brackets(std::numeric_limits<float>::infinity(),.5F,k),"packed finite oracle");
 }
 const auto reference=p.run(0,0,&c,0);native_format_oracles(reference,c);
 std::cout<<"native-format independent CPU reduction/preparation oracles passed"<<std::endl;
 const auto adapted=p.run(0,0,&c,1);
 std::array<double,7> writers{};bool equal=true;
 for(unsigned i=0;i<7;++i){writers[i]=i==5?(alpha_equal(reference.stages[i],adapted.stages[i])?0:1):delta(reference.stages[i],adapted.stages[i]);equal=equal&&writers[i]==0;}
 const auto output_delta=delta(reference.output,adapted.output);
 const auto wrong=p.run(0,0,&c,2),stale=p.run(0,0,&c,3),outside=p.run(0,0,&c,4);
 const auto wrong_delta=delta(reference.output,wrong.output),stale_delta=delta(reference.output,stale.output),outside_delta=delta(reference.output,outside.output);
 require(wrong_delta>1e-5&&stale_delta>1e-5,"native-format negative controls sensitive");
 save(out/L"reference-output.bin",std::as_bytes(std::span(reference.output)));save(out/L"adapted-output.bin",std::as_bytes(std::span(adapted.output)));
 for(unsigned i=0;i<7;++i){save(out/("reference-writer-"+std::to_string(i)+".bin"),std::as_bytes(std::span(reference.stages[i])));save(out/("adapted-writer-"+std::to_string(i)+".bin"),std::as_bytes(std::span(adapted.stages[i])));}
 std::ofstream report(out/L"result.json");
 report<<"{\"schema\":3,\"captured_resource_formats\":true,\"captured_constants\":true,\"synthetic_pixels\":true,\"descriptor_contents_verified\":false,\"live_contract_verified\":false,\"gpu_validation\":true,\"vendor\":"<<p.vendor
  <<",\"native_format_equivalence\":"<<((equal&&output_delta==0)?"true":"false")<<",\"cpu_oracles\":true,\"cpu_oracle_rounding\":\"adjacent-representable-bracket\",\"writer_deltas\":[";
 for(unsigned i=0;i<7;++i){if(i)report<<',';report<<writers[i];}
 report<<"],\"output_maximum_delta\":"<<output_delta<<",\"wrong_color_delta\":"<<wrong_delta<<",\"stale_writer_delta\":"<<stale_delta<<",\"outside_depth_delta\":"<<outside_delta<<",\"format_ids\":[";
 for(unsigned i=0;i<dof_formats::formats.size();++i){if(i)report<<',';report<<unsigned(dof_formats::formats[i]);}
 report<<"],\"experiment_completed\":true}";require(bool(report),"native-format report");
 std::cout<<"PASS native-format measurement: output="<<output_delta<<" wrong-color="<<wrong_delta<<" stale-writer="<<stale_delta<<" outside-depth="<<outside_delta<<std::endl;
}

// Included by the offline whole-pipeline fixture after Pipeline/Result.
bool alpha_equal(const std::vector<float> &a,const std::vector<float> &b){
 // Pixels stores tightly packed readback bytes in float-sized storage. R8
 // alpha bytes can form NaN bit patterns; compare their representation only.
 return a.size()==b.size()&&std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;
}
void captured_oracles(const Result &r,const Captured &c){
 const auto w=c.width,h=c.height;const auto blur=c.cb.data()+384;
 for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w/20;++x){
  const float d=captured_depth(x*20,y,c.view_width,c.view_height,false);
  const float linear=c.cb[106]/std::fma(-d,c.cb[105],c.cb[104]);
  const float radius=std::abs(std::fma(blur[1],d,blur[0]));const auto at=(size_t(y)*(w/20)+x)*4;
  require(std::abs(r.stages[0][at]-linear)<.0001F*(1+linear)&&std::abs(r.stages[0][at+1]-radius)<1e-6F&&std::abs(r.stages[0][at+2]-radius)<1e-6F,"captured horizontal CPU oracle");
 }
 for(unsigned y=0;y<h/20;++y)for(unsigned x=0;x<w/20;++x){
  const auto src=(size_t(20*y)*(w/20)+x)*4,dst=(size_t(y)*(w/20)+x)*4;
  for(unsigned k=0;k<3;++k)require(r.stages[0][src+k]==r.stages[1][dst+k],"captured vertical CPU oracle");
 }
 for(unsigned y=0;y<h/2;++y)for(unsigned x=0;x<w/2;++x){
  // Native prepare clamps its 2x2 render-space footprint to iViewSize-2.
  // The independent proxy is a CPU 2x2 box of the reconstructed scene, so
  // preparation averages sixteen display pixels. All stimulus values and
  // sums are exactly representable, independent of addition order.
  const unsigned sx=2*(std::min)(2*x,c.view_width-2),sy=2*(std::min)(2*y,c.view_height-2);
  std::array<float,3> rgb{};for(unsigned dy=0;dy<4;++dy)for(unsigned dx=0;dx<4;++dx){const auto v=scene(sx+dx,sy+dy);for(unsigned k=0;k<3;++k)rgb[k]+=v[k]/16.F;}
  for(unsigned k=0;k<3;++k)require(r.stages[4][(size_t(y)*(w/2)+x)*4+k]==rgb[k],"captured preparation CPU oracle");
 }
}
void captured_cases(Pipeline &p,const Captured &c,const std::filesystem::path &out,const std::filesystem::path &shaders){
 const auto original=read(shaders/(std::string(hashes[3])+".cs.dxbc"));const auto adapted_code=dof_experiment::prepare(original);
 save(out/L"prepare-adapted.dxbc",adapted_code);disassemble(out/L"prepare-adapted.asm",adapted_code);
 auto corrupt=original;corrupt.back()^=std::byte{1};rejects([&]{dof_experiment::prepare(corrupt);});
 rejects([&]{dof_experiment::prepare(std::span(original).first(original.size()-4));});
 std::cout<<"captured reference "<<c.width<<'x'<<c.height<<" view "<<c.view_width<<'x'<<c.view_height<<std::endl;
 const auto reference=p.run(0,0,&c,0);captured_oracles(reference,c);
 std::cout<<"independent CPU reduction/preparation oracles passed"<<std::endl;
 const auto adapted=p.run(0,0,&c,1);captured_oracles(adapted,c);
 std::array<double,7> stage_delta{};
 for(unsigned i=0;i<7;++i){
  if(i==5)require(alpha_equal(reference.stages[i],adapted.stages[i]),"alpha bytes identical");
  else {stage_delta[i]=delta(reference.stages[i],adapted.stages[i]);require(stage_delta[i]==0,"compute writer reference mismatch");}
 }
 const auto output_delta=delta(reference.output,adapted.output);require(output_delta<1e-5,"whole pipeline composition reference mismatch");
 std::cout<<"adapted pipeline matched, maximum delta "<<output_delta<<std::endl;
 const auto wrong=p.run(0,0,&c,2);const auto wrong_delta=delta(reference.output,wrong.output);
 require(wrong_delta>1e-5,"unscaled color read negative control");
 const auto stale=p.run(0,0,&c,3);const auto stale_delta=delta(reference.output,stale.output);
 require(stale_delta>1e-5,"chronological Image overwrite negative control");
 const auto outside=p.run(0,0,&c,4);const auto outside_delta=delta(reference.output,outside.output);
 // Observe sensitivity rather than asserting the native shader ignores this
 // region. Its reduction dispatch spans the full depth allocation.
 require(delta(reference.stages[4],outside.stages[4])==0,"view-clamped color preparation footprint");
 save(out/L"reference-output.bin",std::as_bytes(std::span(reference.output)));
 save(out/L"adapted-output.bin",std::as_bytes(std::span(adapted.output)));
 save(out/L"reference-prepared-color.bin",std::as_bytes(std::span(reference.stages[4])));
 std::ofstream report(out/L"result.json");
 report<<"{\"schema\":2,\"captured_constants\":true,\"synthetic_pixels\":true,\"native_format_equivalence\":false,\"live_contract_verified\":false,\"gpu_validation\":true,\"vendor\":"<<p.vendor
  <<",\"allocation\":["<<c.width<<','<<c.height<<"],\"view\":["<<c.view_width<<','<<c.view_height<<"],\"cpu_reduction_oracle\":true,\"cpu_preparation_oracle\":true,\"constants_preserved_in_adapter\":true,\"compute_writers_equal\":true,\"output_maximum_delta\":"<<output_delta
  <<",\"unscaled_color_negative_delta\":"<<wrong_delta<<",\"stale_writer_negative_delta\":"<<stale_delta<<",\"outside_depth_footprint_delta\":"<<outside_delta<<",\"outside_writer_delta\":[";
 for(unsigned i=0;i<7;++i){if(i)report<<',';report<<(i==5?(alpha_equal(reference.stages[i],outside.stages[i])?0:1):delta(reference.stages[i],outside.stages[i]));}
 report<<"],\"constant_sha256\":[";
 for(unsigned i=0;i<3;++i){if(i)report<<',';report<<'\"'<<digest(std::as_bytes(std::span(c.cb)).subspan(i==0?0:i==1?1280:1536,i==0?1072:i==1?112:96))<<'\"';}
 report<<"],\"writer_sha256\":[";
 for(unsigned i=0;i<7;++i){if(i)report<<',';report<<'\"'<<digest(std::as_bytes(std::span(reference.stages[i])))<<'\"';}
 report<<"],\"output_sha256\":\""<<digest(std::as_bytes(std::span(reference.output)))<<"\",\"accepted\":true}";
 require(bool(report),"captured report");std::cout<<"PASS captured constants: 5 complete pipelines, wrong-color="<<wrong_delta<<" stale-writer="<<stale_delta<<" outside-depth="<<outside_delta<<std::endl;
}

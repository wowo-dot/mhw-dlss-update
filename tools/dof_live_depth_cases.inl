// Complete captured depth, paired constants, synthetic HDR color and explicit
// fixture descriptors. An optional horizontal synchronization correction is
// shared by reference/candidate; it changes no math or pixel coordinates.
void live_depth_oracles(const Result &r,const Captured &c,const CapturedDepth &depth){
 const unsigned tw=c.width/20,th=c.height/20;const auto blur=c.cb.data()+384;
 // Captured flags/coefficients are gated by Captured. With both ignores true
 // and both coefficients 1, every depth contributes unmodified linear depth
 // and absolute CoC. Reduce all 20 values; do not use the constant-tile oracle.
 for(unsigned y=0;y<c.height;++y)for(unsigned x=0;x<tw;++x){
  std::array<dof_depth_oracle::Interval,3> expected{{{0,0},{0,0},{1000,1000}}};
  for(unsigned i=0;i<20;++i){const auto d=depth.at(x*20+i,y,c);
   const auto linear=dof_depth_oracle::divide(c.cb[106],dof_depth_oracle::mad(-d,c.cb[105],c.cb[104])),radius=dof_depth_oracle::absolute(dof_depth_oracle::mad(blur[1],d,blur[0]));
   expected[0]={(std::max)(expected[0].low,linear.low),(std::max)(expected[0].high,linear.high)};
   expected[1]={(std::max)(expected[1].low,radius.low),(std::max)(expected[1].high,radius.high)};
   expected[2]={(std::min)(expected[2].low,radius.low),(std::min)(expected[2].high,radius.high)};
  }
  for(unsigned k=0;k<3;++k){const auto actual=r.stages[0][(static_cast<std::size_t>(y)*tw+x)*4+k];
   if(!dof_depth_oracle::packed(actual,expected[k],k)){
    std::cerr<<"depth horizontal "<<x<<','<<y<<" channel "<<k<<" actual="<<actual<<" expected=["<<expected[k].low<<','<<expected[k].high<<"]\n";
    require(false,"live-depth horizontal CPU oracle");}
  }
 }
 // Vertical reduction operates on the already rounded horizontal image.
 for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){
  std::array<float,3> expected{0,0,1000};
  for(unsigned i=0;i<20;++i){const auto at=(static_cast<std::size_t>(y*20+i)*tw+x)*4;
   expected[0]=(std::max)(expected[0],r.stages[0][at]);expected[1]=(std::max)(expected[1],r.stages[0][at+1]);expected[2]=(std::min)(expected[2],r.stages[0][at+2]);}
  for(unsigned k=0;k<3;++k)require(r.stages[1][(static_cast<std::size_t>(y)*tw+x)*4+k]==expected[k],"live-depth vertical CPU oracle");
 }
}
void live_depth_cases(Pipeline &p,const Captured &c,const CapturedDepth &depth,const std::filesystem::path &out){
 const auto sixteen=dof_depth_oracle::divide(1499639.625F,dof_depth_oracle::mad(0,93726.4765625F,93727.4765625F));
 require(dof_depth_oracle::packed(16.F,sixteen,0)&&dof_depth_oracle::packed(15.875F,sixteen,0)&&!dof_depth_oracle::packed(15.75F,sixteen,0),"division boundary oracle controls");
 require(!dof_depth_oracle::packed(.00050354F,{.000423083,.000423085},2),"missing reduction values rejected");
 const auto reference=p.run(0,0,&c,0,&depth);
 // Preserve the native reference even if a later oracle rejects it.
 save(out/L"reference-output.bin",std::as_bytes(std::span(reference.output)));
 save(out/L"depth-readback.r32f",std::as_bytes(std::span(reference.depth)));
 for(unsigned i=0;i<7;++i)save(out/("reference-writer-"+std::to_string(i)+".bin"),std::as_bytes(std::span(reference.stages[i])));
 live_depth_oracles(reference,c,depth);
 std::cout<<"live-depth CPU reductions passed"<<std::endl;
 const auto adapted=p.run(0,0,&c,1,&depth);live_depth_oracles(adapted,c,depth);
 std::array<double,7> writers{};bool equal=true;
 for(unsigned i=0;i<7;++i){writers[i]=i==5?(alpha_equal(reference.stages[i],adapted.stages[i])?0:1):delta(reference.stages[i],adapted.stages[i]);equal=equal&&writers[i]==0;}
 const auto output_delta=delta(reference.output,adapted.output);
 // Keep negative controls as measurements. A captured shallow blur can make
 // the final image insensitive to a wrong intermediate; synthetic cases test
 // the strong-blur controls separately. Never use these as equality tolerances.
 const auto wrong=p.run(0,0,&c,2,&depth),stale=p.run(0,0,&c,3,&depth),outside=p.run(0,0,&c,4,&depth);
 const auto wrong_delta=delta(reference.output,wrong.output),stale_delta=delta(reference.output,stale.output),outside_delta=delta(reference.output,outside.output);
 const auto wrong_prepare=delta(reference.stages[4],wrong.stages[4]);
 const auto outside_horizontal=delta(reference.stages[0],outside.stages[0]);
 require(wrong_prepare>0&&outside_horizontal>0,"input negative controls sensitive");
 save(out/L"adapted-output.bin",std::as_bytes(std::span(adapted.output)));
 for(unsigned i=0;i<7;++i)save(out/("adapted-writer-"+std::to_string(i)+".bin"),std::as_bytes(std::span(adapted.stages[i])));
 std::ofstream report(out/L"result.json");
 report<<"{\"schema\":4,\"captured_resource_formats\":true,\"captured_constants\":true,\"captured_depth_pixels\":true,\"synthetic_color\":true,\"native_descriptor_contents_verified\":false,\"live_contract_verified\":false,\"gpu_validation\":"<<(p.gpu_validation?"true":"false")<<",\"synchronized_horizontal_reduction\":"<<(p.sync_reduction?"true":"false")<<",\"vendor\":"<<p.vendor
  <<",\"depth_sha256\":\""<<depth.sha256<<"\",\"constant_pack_sha256\":\""<<digest(std::as_bytes(std::span(c.cb)))<<"\",\"depth_pixels_bit_exact\":"<<depth.pixels.size()
  <<",\"cpu_reduction_oracles\":true,\"cpu_oracle_rounding\":\"mad-div-operation-interval-and-packed-bracket\",\"compute_writers_equal\":"<<(equal?"true":"false")<<",\"writer_deltas\":[";
 for(unsigned i=0;i<7;++i){if(i)report<<',';report<<writers[i];}
 report<<"],\"output_maximum_delta\":"<<output_delta<<",\"wrong_color_delta\":"<<wrong_delta<<",\"wrong_preparation_delta\":"<<wrong_prepare<<",\"stale_writer_delta\":"<<stale_delta<<",\"outside_depth_delta\":"<<outside_delta<<",\"outside_horizontal_delta\":"<<outside_horizontal
  <<",\"output_sha256\":\""<<digest(std::as_bytes(std::span(reference.output)))<<"\",\"experiment_completed\":true,\"accepted\":"<<((equal&&output_delta==0)?"true":"false")<<"}";
 require(bool(report),"live-depth report");report.close();
 require(equal&&output_delta==0,"live-depth pipeline reference mismatch");
 std::cout<<"PASS captured full-depth pipeline: output="<<output_delta<<" wrong-color="<<wrong_delta<<" stale-writer="<<stale_delta<<" cleared-outside="<<outside_delta<<std::endl;
}

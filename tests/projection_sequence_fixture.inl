// Exercise real core hooks: later stimuli wait for the selected draw and each
// mutation restores independently, including failure/stop during a later call.
void sequence_case(int argc,char **argv) {
  next_jitter_frame(0);invoke(pb::producer,camera+0x50,context);
  next_jitter_frame(1);invoke(pb::producer,camera+0x50,context);
  invoke(pb::publish,context,0);invoke(pb::consume,address(executor),address(backend_context),address(packet));
  check(pb::status().phase==pb::Phase::consumed && original_jitter_calls==1,"sequence initial chain");
  if(mode=="jitter_sequence_wait_cap") {
    for(unsigned i=0;i<=pb::max_draw_waits;++i)invoke(pb::producer,camera+0x50,context);
    const auto c=pb::finished_capture();
    check(c && pb::status().stop==pb::Stop::capacity && c->stimulus.draw_waits==65 && c->stimulus.injections==1 && c->stimulus.restorations==1,"draw wait cap");
    invoke(pb::producer,camera+0x50,context);
    check(pb::finished_capture()->stimulus.draw_waits==65,"post-stop draw wait accounting");
    check(pb::remove_owned(),"draw wait cleanup");return;
  }
  // Do not change the primary arena contents while its draw is pending.
  invoke(pb::producer,camera+0x50,context);
  check(original_jitter_calls==1 && pb::status().fault==pb::Fault::none,"later stimulus before draw");
  invoke(pb::draw0,address(executor),address(backend_context),0x2222);
  check(pb::status().phase==pb::Phase::drawn && pb::provenance::owned_draw_return_calls()==1,"sequence draw return");
  for(unsigned sample=2;sample<5;++sample) {
    next_jitter_frame(sample);
    const auto native=sample==2?std::uint64_t{bits(-.25F)}|(std::uint64_t{bits(.125F)}<<32):
      std::uint64_t{bits(.125F)}|(std::uint64_t{bits(.375F)}<<32);
    put(address(table),0xe6a0+8*(10+sample),native);
    if(sample==2 && mode=="jitter_sequence_repeat")put(address(table),0xe6a0+8*(10+sample),std::uint64_t{bits(.25F)}|(std::uint64_t{bits(-.375F)}<<32));
    if(sample==2 && mode=="jitter_sequence_zero")put(address(table),0xe6a0+8*(10+sample),std::uint64_t{});
    if(sample==4 && mode=="jitter_sequence_recovery_changed")put(context,0x230,bits(1920.F));
    if(sample==4 && mode=="jitter_sequence_deferred") {
      check(pb::hold_owned(pb::consume,false),"sequence recovery lock");
      invoke(pb::producer,camera+0x50,context);pb::release_owned();
      check(pb::status().fault==pb::Fault::none,"sequence recovery deferral became fatal");
    }
    if(sample==2 && mode=="jitter_sequence_exception")check(exception(),"late exception forwarding");
    else invoke(pb::producer,camera+0x50,context);
    std::uint64_t restored{};std::memcpy(&restored,reinterpret_cast<void *>(camera+0x38),8);
    check(restored==(mode=="jitter_sequence_conflict"?1234U:0U),"late sequence restoration");
    if(pb::status().stop!=pb::Stop::recording)break;
  }
  const auto c=pb::finished_capture();check(c && pb::settled(),"sequence did not settle");
  const bool accepted=mode=="jitter_sequence_chain" || mode=="jitter_sequence_deferred";
  check(accepted?(pb::status().stop==pb::Stop::complete && pb::status().fault==pb::Fault::none):
    mode=="jitter_sequence_stop"?pb::status().stop==pb::Stop::requested:pb::status().fault!=pb::Fault::none,"sequence verdict");
  check(c->stimulus.injections<=3 && c->stimulus.restorations+(mode=="jitter_sequence_conflict"?1:0)==c->stimulus.injections,"sequence budget/restore");
  if(accepted) {
    check(c->stimulus.count==5 && c->stimulus.injections==3 && c->stimulus.restorations==3 && original_jitter_calls==3,"sequence completion");
    check(c->stimulus.draw_waits==1 && c->stimulus.recovery_deferrals==(mode=="jitter_sequence_deferred"?1U:0U),"sequence wait accounting");
    for(unsigned i=0;i<5;++i)check(c->stimulus.calls[i].complete && c->stimulus.calls[i].restored &&
      c->stimulus.calls[i].injected==(i>0 && i<4),"sequence sample classification");
    check(c->source.order==c->stimulus.calls[1].source.order && c->stimulus.calls[2].source.order>c->draws[0].exit_order,"primary binding overwritten");
  }
  const auto before=original_jitter_calls;if(mode!="jitter_sequence_conflict")invoke(pb::producer,camera+0x50,context);
  check(original_jitter_calls==before && pb::provenance::owned_draw_return_calls()==1,"post-stop injection/marker");
  check(pb::remove_owned(),"sequence hook cleanup");
  if(argc>2 && accepted) {
    const std::filesystem::path path(argv[2]);check(!std::filesystem::exists(path),"preserve sequence fixture");
    std::ofstream out(path);out<<std::boolalpha;pb::write_capture(out,*c,path.wstring());check(static_cast<bool>(out),"sequence fixture write");
  }
}

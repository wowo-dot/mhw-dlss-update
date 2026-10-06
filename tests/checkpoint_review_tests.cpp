#include "checkpoint_review.hpp"
#include "../src/exception_capture/capture_retry.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace b = mhw::bridge_probe;
namespace ec = mhw::exception_capture;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
  b::MailboxRead capture;
  b::JournalRead journal;
  Fixture() {
    capture.state = b::MailboxState::published; capture.inputs_verified = true;
    capture.binding = {123, {1000,1001,0,1003,1004}, 7, 0};
    journal.valid = true; journal.state = 2; journal.completed = 4; journal.deadline = 2000;
    journal.checkpoint = 2; journal.checkpoint_reserved = 3; journal.checkpoint_prefix = 1;
    journal.evaluate_command = 123; journal.evaluate_order = 8;
    journal.checkpoints[0] = {2,3,1,123,1000,8};
    b::BarrierCall reset{}; reset.command = 123; reset.sequence = 1; reset.kind = b::BarrierCallKind::reset;
    reset.entry_order = 1; reset.return_order = 2; reset.result = S_OK;
    auto crossing = reset; crossing.command = 999; crossing.sequence = 2; crossing.kind = b::BarrierCallKind::barrier;
    crossing.entry_order = 3; crossing.return_order = 9; crossing.first = 0; crossing.count = 1;
    auto selected = reset; selected.sequence = 3; selected.kind = b::BarrierCallKind::barrier;
    selected.entry_order = 4; selected.return_order = 5; selected.first = 1; selected.count = 4;
    auto after = reset; after.sequence = 4; after.entry_order = 10; after.return_order = 11; after.kind = b::BarrierCallKind::close;
    journal.records = {reset,crossing,selected,after};
    for (auto resource : {9999ULL,1000ULL,1001ULL,1003ULL,1004ULL}) {
      D3D12_RESOURCE_BARRIER v{}; v.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      v.Transition = {reinterpret_cast<ID3D12Resource *>(resource), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
          D3D12_RESOURCE_STATE_RENDER_TARGET, resource == 1004 ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
      journal.barrier_records.push_back(v);
    }
    journal.descriptions = 5;
  }
  b::CheckpointReview review() const { return b::review_checkpoint(capture, journal, 0); }
};
void tests() {
  Fixture f;
  const auto good = f.review();
  check(good.accepted && good.epoch == 1 && good.unrelated_overlaps == 1 && good.selected_calls == 2,
        "unrelated crossing may finish later; post-marker Close excluded; original prefix remains incomplete");
  check(!f.capture.qualified && f.journal.checkpoints[0].prefix < f.journal.checkpoints[0].reserved, "original observation remains unchanged");
  auto unknown=f;
  unknown.journal.barrier_records[1].Transition.pResource=reinterpret_cast<ID3D12Resource *>(5555);
  check(unknown.review().accepted && unknown.review().unknown_state_mask==1, "absent color transition stays unknown during diagnostic capture");
  unknown.journal.barrier_records[3].Transition.pResource=reinterpret_cast<ID3D12Resource *>(5556);
  check(!unknown.review().accepted, "depth read declaration required for this bounded diagnostic");
  for (unsigned defect = 0; defect < 25; ++defect) {
    Fixture bad;
    switch (defect) {
    case 0: bad.capture.inputs_verified = false; break;
    case 1: bad.capture.state = b::MailboxState::writing; break;
    case 2: bad.capture.binding.index = 1; break;
    case 3: bad.capture.binding.command = 124; break;
    case 4: bad.capture.binding.thread = 0; break;
    case 5: bad.capture.binding.resources[3] = 0; break;
    case 6: bad.journal.pending = 1; break;
    case 7: bad.journal.failed = 1; break;
    case 8: bad.journal.flags = b::journal_overflow; break;
    case 9: bad.journal.valid = false; break;
    case 10: bad.journal.records[1].command = 123; break;
    case 11: bad.journal.barrier_records[0].Transition.pResource = reinterpret_cast<ID3D12Resource *>(1003); break;
    case 12: bad.journal.barrier_records[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; bad.journal.barrier_records[0].UAV.pResource = nullptr; break;
    case 13: bad.journal.barrier_records[0].Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING; break;
    case 14: bad.journal.barrier_records[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY; break;
    case 15: bad.journal.barrier_records[0].Type = static_cast<D3D12_RESOURCE_BARRIER_TYPE>(99); break;
    case 16: bad.journal.records[1].kind = b::BarrierCallKind::bundle; break;
    case 17: bad.journal.records[1].count = 99999; break;
    case 18: bad.journal.records[1].sequence = 3; break;
    case 19: bad.journal.records[1].return_order = 8; break;
    case 20: bad.journal.records[0].result = E_FAIL; break;
    case 21: bad.journal.records[2].kind = b::BarrierCallKind::close; break;
    case 22: bad.journal.barrier_records[3].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST; break;
    case 23: bad.journal.barrier_records[4].Transition.Subresource = 0; break;
    case 24: bad.journal.records[0].return_order = 6; break;
    }
    check(!bad.review().accepted, "unsafe, incomplete, misbound or malformed checkpoint rejected");
  }
  auto query = f; query.journal.records[1].kind = b::BarrierCallKind::fence_completed; query.journal.records[1].command = 0;
  query.journal.records[1].count = 0;
  check(query.review().accepted && query.review().query_overlaps == 1, "read-only fence query overlap accepted separately");
  auto uav = f; uav.journal.barrier_records[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  uav.journal.barrier_records[0].UAV.pResource = reinterpret_cast<ID3D12Resource *>(9999);
  check(uav.review().accepted, "explicit unrelated UAV allowed");
  for (auto kind : {b::BarrierCallKind::execute,b::BarrierCallKind::signal,b::BarrierCallKind::wait,b::BarrierCallKind::fence_cpu_signal,
                    b::BarrierCallKind::reset,b::BarrierCallKind::close}) {
    auto bad = f; bad.journal.records[1].kind = kind;
    check(!bad.review().accepted, "other crossing operations remain unclassified and rejected");
  }
  std::array<b::MailboxRead,3> captures{};
  for (auto &c : captures) c.state = b::MailboxState::idle;
  captures[0] = f.capture;
  check(ec::retry_sequence_ready(1,captures,f.journal,1000), "scoped review replaces only global-prefix rejection");
  check(!ec::retry_sequence_ready(2,captures,f.journal,1000), "first capture cannot substitute for second");
  check(!ec::retry_sequence_ready(0,captures,f.journal,1000), "extra pre-correction capture rejected");
  for (unsigned i=0;i<5;++i) {
    auto bad=f.journal;
    if (i==0) bad.deadline=1000;
    if (i==1) bad.state=3;
    if (i==2) bad.completed=b::kBarrierCallCapacity;
    if (i==3) bad.descriptions=b::kBarrierDescriptionCapacity;
    if (i==4) bad.lists=b::kSubmittedListCapacity;
    check(!ec::retry_sequence_ready(1,captures,bad,1000), "time/state/headroom bounds preserved");
  }
}
// Bounded textual replay generated locally from a saved capture. Pointer labels
// are never dereferenced; proprietary capture files are not checked into Git.
void replay(const char *path) {
  std::ifstream in(path); check(in.good(), "open replay"); Fixture f;
  unsigned version{}, count{}, barriers{}; in >> version; check(version==1,"replay version");
  auto &binding=f.capture.binding; in >> binding.command >> binding.thread >> binding.index;
  for (auto &p:binding.resources) in >> p;
  auto &m=f.journal.checkpoints[0]; in >> m.state >> m.reserved >> m.prefix >> m.command >> m.tick >> m.order;
  in >> count >> barriers; check(count<=b::kBarrierCallCapacity && barriers<=b::kBarrierDescriptionCapacity,"replay bounds");
  f.journal.records.resize(count); f.journal.barrier_records.resize(barriers); f.journal.completed=count;
  for (auto &c:f.journal.records) {
    unsigned kind{}, result{};
    in >> c.command >> c.caller >> c.tick >> c.thread >> c.sequence >> c.first >> c.count >> kind >> result >> c.queue >> c.fence >> c.fence_value >> c.entry_order >> c.return_order;
    c.kind=static_cast<b::BarrierCallKind>(kind); c.result=static_cast<HRESULT>(result);
  }
  for (auto &v:f.journal.barrier_records) {
    unsigned type{},flags{},sub{},before{},after{}; std::uintptr_t a{},z{};
    in >> type >> flags >> a >> z >> sub >> before >> after; v.Type=static_cast<D3D12_RESOURCE_BARRIER_TYPE>(type); v.Flags=static_cast<D3D12_RESOURCE_BARRIER_FLAGS>(flags);
    if(type==0) v.Transition={reinterpret_cast<ID3D12Resource *>(a),sub,static_cast<D3D12_RESOURCE_STATES>(before),static_cast<D3D12_RESOURCE_STATES>(after)};
    if(type==1) v.Aliasing={reinterpret_cast<ID3D12Resource *>(a),reinterpret_cast<ID3D12Resource *>(z)};
    if(type==2) v.UAV.pResource=reinterpret_cast<ID3D12Resource *>(a);
  }
  check(!in.fail(),"complete replay data"); std::string extra; check(!(in>>extra),"no trailing replay data");
  const auto r=f.review();
  std::cout << "Replay review: accepted=" << r.accepted << " reason=" << r.reason << " epoch=" << r.epoch << " overlaps=" << r.unrelated_overlaps << '\n';
  check(r.accepted && r.epoch==215 && r.unrelated_overlaps==1 && r.expected_state_mask==10 && r.unknown_state_mask==17,
        "run 49944: unrelated overlap accepted; motion/depth reads known, color/output states still unknown");
  std::cout << "PASS saved run 49944: epoch=" << r.epoch << " unrelated_overlaps=" << r.unrelated_overlaps << " selected_calls=" << r.selected_calls << '\n';
  auto hazard=f;
  hazard.journal.barrier_records[hazard.journal.records[263].first].Transition.pResource=reinterpret_cast<ID3D12Resource *>(binding.resources[3]);
  check(!hazard.review().accepted,"actual captured trace with one overlapping depth label changed is rejected");
}
int main(int argc,char **argv) {
  try { tests(); if(argc==2) replay(argv[1]); std::cout << "Checkpoint review and sequence bounds passed.\n"; return 0; }
  catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

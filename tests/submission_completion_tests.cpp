#include "submission_completion.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace b = mhw::bridge_probe;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
std::vector<b::BarrierCall> fixture() {
  std::vector<b::BarrierCall> v(7);
  for (unsigned i = 0; i < v.size(); ++i) {
    v[i].sequence = i + 1; v[i].entry_order = 10 * i + 1; v[i].return_order = 10 * i + 2; v[i].result = S_OK;
  }
  v[0].kind = b::BarrierCallKind::reset; v[0].command = 100;
  v[1].kind = b::BarrierCallKind::barrier; v[1].command = 100;
  v[2].kind = b::BarrierCallKind::close; v[2].command = 100;
  v[3].kind = b::BarrierCallKind::execute; v[3].queue = 200; v[3].count = 2;
  v[4].kind = b::BarrierCallKind::signal; v[4].queue = 200; v[4].fence = 300; v[4].fence_value = 7;
  v[5].kind = b::BarrierCallKind::fence_completed; v[5].fence = 300; v[5].fence_value = 6;
  v[6].kind = b::BarrierCallKind::fence_completed; v[6].fence = 300; v[6].fence_value = 7;
  return v;
}
int main(int argc, char **argv) {
  try {
    const std::array<std::uintptr_t, 2> lists{99, 100};
    for (unsigned bad = 0; bad < 16; ++bad) {
      auto v = fixture();
      if (bad == 1) v[0].result = E_FAIL;
      if (bad == 2) v[0].kind = b::BarrierCallKind::barrier;
      if (bad == 3) v[1].kind = b::BarrierCallKind::bundle;
      if (bad == 4) { v[1].kind = b::BarrierCallKind::reset; v[1].entry_order = 18; v[1].return_order = 19; }
      if (bad == 5) v[2].return_order = 35;
      if (bad == 6) v[1].return_order = 18;
      if (bad == 7) v[2].result = E_FAIL;
      if (bad == 8) v[3].count = 200;
      if (bad == 9) v[4].result = E_FAIL;
      if (bad == 10) v[5].fence_value = ~std::uint64_t{};
      if (bad == 11) { v[5].kind = b::BarrierCallKind::fence_cpu_signal; v[5].fence_value = 7; }
      if (bad == 12) { v[5].kind = b::BarrierCallKind::signal; v[5].queue = 201; }
      if (bad == 13) { v[5].kind = b::BarrierCallKind::signal; v[5].queue = 200; v[5].fence_value = 2; }
      if (bad == 14) { v[5].kind = b::BarrierCallKind::wait; v[5].queue = 200; v[5].entry_order = 30; v[5].return_order = 33; }
      if (bad == 15) v[6].sequence = 8;
      const auto r = b::review_submission_completion(v, lists, 100, 17);
      if (r.status != (bad ? b::CompletionStatus::rejected : b::CompletionStatus::observed))
        std::cerr << "case " << bad << " status " << static_cast<unsigned>(r.status) << " issue " << static_cast<unsigned>(r.issue) << '\n';
      check(r.status == (bad ? b::CompletionStatus::rejected : b::CompletionStatus::observed), "bad chain accepted or valid chain rejected");
      if (!bad) check(r.reset == 1 && r.close == 3 && r.submission == 4 && r.signal == 5 && r.completed == 7 && r.value == 7, "bound event chain");
    }
    auto v = fixture();
    check(b::review_submission_completion(std::span(v.data(), 6), lists, 100, 17).status == b::CompletionStatus::pending, "low value stays pending");
    v[4].queue = 201;
    check(b::review_submission_completion(v, lists, 100, 17).status == b::CompletionStatus::pending, "foreign queue cannot complete target");
    v = fixture(); v[6].command = 100; v[6].kind = b::BarrierCallKind::reset;
    check(b::review_submission_completion(v, lists, 100, 17).status == b::CompletionStatus::pending, "later reset doesn't invent a completed-value query");
    v = fixture();
    auto late = v[5]; late.sequence = 8; late.kind = b::BarrierCallKind::fence_cpu_signal;
    late.entry_order = 60; late.return_order = 70; late.fence_value = 7;
    check(b::review_submission_completion(v, lists, 100, 17).status == b::CompletionStatus::observed, "candidate before later slot drains");
    v.push_back(late);
    check(b::review_submission_completion(v, lists, 100, 17).status == b::CompletionStatus::rejected, "late-published competing writer invalidates candidate during drain");
    if (argc == 2) {
      std::ifstream input(argv[1], std::ios::binary);
      unsigned n{}, l{}; std::uint64_t command{}, marker{};
      input.read(reinterpret_cast<char *>(&n), 4); input.read(reinterpret_cast<char *>(&l), 4);
      input.read(reinterpret_cast<char *>(&command), 8); input.read(reinterpret_cast<char *>(&marker), 8);
      check(input.good() && n <= 2048 && l <= 4096, "bounded saved fixture header");
      std::vector<b::BarrierCall> calls(n); std::vector<std::uintptr_t> saved_lists(l);
      input.read(reinterpret_cast<char *>(calls.data()), n * sizeof(b::BarrierCall));
      input.read(reinterpret_cast<char *>(saved_lists.data()), l * sizeof(std::uintptr_t));
      check(input.good(), "complete saved fixture");
      for (unsigned prefix : {596U, n}) {
        check(prefix <= n, "fixture prefix available");
        const auto r = b::review_submission_completion(std::span(calls.data(), prefix), saved_lists, command, marker);
        check(r.status == b::CompletionStatus::observed && r.submission == 395 && r.signal == 397 && r.completed == 596 && r.value == 11522,
            "saved run 47820 submission/fence replay");
      }
      std::cout << "Saved run 47820: submission 395, signal 397, query 596; original overflow is not waived.\n";
    }
    std::cout << "Submission completion counterexamples passed.\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}

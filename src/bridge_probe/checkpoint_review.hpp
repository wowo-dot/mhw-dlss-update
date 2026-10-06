#pragma once
#include "capture_mailbox.hpp"
#include "barrier_journal.hpp"
#include <string_view>

namespace mhw::bridge_probe {
struct CheckpointReview {
  bool accepted{};
  std::string_view reason{"unreviewed"};
  unsigned epoch{}, selected_calls{}, unrelated_overlaps{}, query_overlaps{};
  unsigned declared_state_mask{}, expected_state_mask{}, unknown_state_mask{};
};
// External recorder only, with all target threads stopped. Reconstructs CPU
// declarations from immutable completed records; never certifies GPU state.
CheckpointReview review_checkpoint(const MailboxRead &, const JournalRead &, unsigned index);
} // namespace mhw::bridge_probe

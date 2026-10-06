#pragma once
#include "../bridge_probe/capture_mailbox.hpp"
#include "../bridge_probe/barrier_journal.hpp"
#include "../bridge_probe/checkpoint_review.hpp"

namespace mhw::exception_capture {
// Called with the target stopped, after both channels have been saved/flushed.
// Transport integrity and a complete checkpoint are observation prerequisites;
// they do not certify resource contents, GPU completion or Evaluate success.
inline bool journal_channels_ready(const bridge_probe::JournalRead &journal, std::uint64_t now) noexcept {
  namespace b = bridge_probe;
  if (!journal.valid || journal.state != 2 || journal.flags || journal.pending || journal.failed ||
      !journal.completed || !now || now >= journal.deadline ||
      journal.completed > b::kBarrierCallCapacity - 512 ||
      journal.descriptions > b::kBarrierDescriptionCapacity - 2048 ||
      journal.lists > b::kSubmittedListCapacity - 512) return false;
  return true;
}
inline bool retry_channels_ready(unsigned attempts, bridge_probe::MailboxState mailbox,
    const bridge_probe::JournalRead &journal, std::uint64_t now) noexcept {
  namespace b = bridge_probe;
  if (!journal_channels_ready(journal, now)) return false;
  if (!attempts) return mailbox == b::MailboxState::idle && journal.checkpoint == 0;
  return mailbox == b::MailboxState::published && journal.checkpoint == 2 &&
      journal.evaluate_command && journal.evaluate_order && journal.checkpoint_prefix &&
      journal.checkpoint_reserved == journal.checkpoint_prefix &&
      journal.completed >= journal.checkpoint_reserved;
}
inline bool retry_sequence_ready(unsigned attempts, const std::array<bridge_probe::MailboxRead, 3> &captures,
    const bridge_probe::JournalRead &journal, std::uint64_t now) {
  namespace b = bridge_probe;
  if (attempts > 3 || !journal_channels_ready(journal, now)) return false;
  for (unsigned i = 0; i < 3; ++i) {
    const auto &c = journal.checkpoints[i];
    if (i >= attempts) {
      if (captures[i].state != b::MailboxState::idle || c.state != 0) return false;
    } else {
      if (!b::review_checkpoint(captures[i], journal, i).accepted ||
          (i && (c.order <= journal.checkpoints[i-1].order || c.reserved <= journal.checkpoints[i-1].reserved))) return false;
    }
  }
  return true;
}
} // namespace mhw::exception_capture

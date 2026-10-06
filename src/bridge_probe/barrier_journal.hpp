#pragma once
#include "barrier_history.hpp"
#include "submission_completion.hpp"
#include <string>
#include <vector>

namespace mhw::bridge_probe {
// Separate append-only channel: each completed slot is published exactly once.
// A read does not claim the journal ended or that any GPU work completed.
inline constexpr unsigned kBarrierJournalProducer = 1500;
enum class JournalStopReason : unsigned { none, requested, completion, expired, overflow, fault, contention, ambiguous };
struct JournalTermination {
  JournalStopReason reason{};
  unsigned flags{}, frontier{}, unused{};
  std::uint64_t order{};
  SubmissionCompletion completion{};
};
enum JournalFlag : unsigned { journal_overflow = 1, journal_fault = 2, journal_contention = 4, journal_expired = 8 };
struct JournalCheckpoint {
  unsigned state{}, reserved{}, prefix{};
  std::uint64_t command{}, tick{}, order{};
};
struct JournalRead {
  std::array<JournalCheckpoint, 3> checkpoints{};
  bool valid{};
  unsigned state{}, pending{}, completed{}, failed{}, flags{};
  unsigned checkpoint{}, checkpoint_reserved{}, checkpoint_prefix{}, descriptions{}, lists{};
  std::uint64_t deadline{}, evaluate_command{}, evaluate_order{};
  bool drained{};
  JournalTermination termination{};
  std::string json;
  // Owned typed copies for external stopped-process checkpoint replay.
  std::vector<BarrierCall> records;
  std::vector<D3D12_RESOURCE_BARRIER> barrier_records;
};
std::wstring barrier_journal_name(DWORD, std::uint64_t);
class BarrierJournalWriter {
 public:
  ~BarrierJournalWriter();
  BarrierJournalWriter() = default;
  BarrierJournalWriter(const BarrierJournalWriter &) = delete;
  BarrierJournalWriter &operator=(const BarrierJournalWriter &) = delete;
  bool create() noexcept;
  bool available() const noexcept { return view_ != nullptr; }
  void start(std::uint64_t deadline) noexcept;
  void stop(unsigned flags) noexcept;
  // Caller has closed admission and observed all admitted callbacks return.
  void terminate(const JournalTermination &) noexcept;
  void reserve(unsigned index) noexcept;
  void publish(unsigned index, const BarrierCall &, std::span<const D3D12_RESOURCE_BARRIER>,
               std::span<const std::uintptr_t>) noexcept;
  void checkpoint(std::uintptr_t command, std::uint64_t tick, std::uint64_t order,
                  unsigned reserved, unsigned prefix, unsigned index = 0) noexcept;
 private:
  HANDLE mapping_{};
  void *view_{};
};
class BarrierJournalReader {
 public:
  ~BarrierJournalReader();
  BarrierJournalReader() = default;
  BarrierJournalReader(const BarrierJournalReader &) = delete;
  BarrierJournalReader &operator=(const BarrierJournalReader &) = delete;
  bool open(DWORD, std::uint64_t);
  JournalRead read() const;
 private:
  HANDLE mapping_{};
  const void *view_{};
  DWORD pid_{};
  std::uint64_t creation_{};
};
} // namespace mhw::bridge_probe

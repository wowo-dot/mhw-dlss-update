#pragma once
#include "barrier_history.hpp"
#include "resource_inspection.hpp"
#include "presentation_observer.hpp"
#include "frame_packet.hpp"
#include <span>
#include <string>

namespace mhw::bridge_probe {
struct JitterSample;
std::size_t format_jitter_sample_json(std::span<char>, const JitterSample &) noexcept;
inline constexpr std::uint32_t kCaptureMailboxSchema = 1, kCaptureMailboxProducer = 1500;
inline constexpr std::size_t kCaptureJsonCapacity = 128 * 1024;
enum class MailboxState : std::uint32_t { unavailable, idle, entered, writing, published, failed, invalid };
struct CaptureMetadata {
  std::uint64_t sequence{}, tick{}, caller{}, token{};
  std::uint32_t thread{}, result{};
  bool committed{}, command_verified{}, boundary_clean{};
  bool handle_record_verified{}, handle_binding_matched{};
  unsigned evaluate_index{};
};
std::size_t format_frame_packet_json(std::span<char>, const FramePacket &) noexcept;
// No allocation, foreign calls or pointer dereferences. Zero means rejected or too small.
std::size_t format_capture_json(std::span<char>, const CaptureMetadata &,
    const EvaluateContractSnapshot *, const ResourceInspection *, const BarrierCapture *,
    const PresentationCreation *, const FramePacket * = nullptr) noexcept;
std::uint64_t process_creation_stamp(HANDLE) noexcept;
std::wstring capture_mailbox_name(DWORD pid, std::uint64_t creation, unsigned index = 0);
struct CaptureBinding {
  std::uint64_t command{};
  std::array<std::uint64_t, kEvaluateResourceKeyCount> resources{};
  unsigned thread{}, index{};
};
static_assert(sizeof(CaptureBinding) == 56);
struct MailboxRead {
  MailboxState state{MailboxState::unavailable}; std::string json;
  bool qualified{}, inputs_verified{};
  CaptureBinding binding{};
};

// One producer, one immutable result. The bridge is process-pinned; the OS closes
// its view on exit. The external reader holds a separate read-only view/handle.
class CaptureMailboxWriter {
 public:
  CaptureMailboxWriter() = default;
  ~CaptureMailboxWriter();
  CaptureMailboxWriter(const CaptureMailboxWriter &) = delete;
  CaptureMailboxWriter &operator=(const CaptureMailboxWriter &) = delete;
  bool create(unsigned index = 0) noexcept;
  bool enter() noexcept;
  bool publish(const CaptureMetadata &, const EvaluateContractSnapshot *,
      const ResourceInspection *, const BarrierCapture *, const PresentationCreation *, const FramePacket * = nullptr) noexcept;
 private:
  HANDLE mapping_{};
  void *view_{};
};
class CaptureMailboxReader {
 public:
  CaptureMailboxReader() = default;
  ~CaptureMailboxReader();
  CaptureMailboxReader(const CaptureMailboxReader &) = delete;
  CaptureMailboxReader &operator=(const CaptureMailboxReader &) = delete;
  bool open(DWORD pid, std::uint64_t creation, unsigned index = 0);
  MailboxRead read() const;
 private:
  HANDLE mapping_{};
  const void *view_{};
  DWORD pid_{};
  std::uint64_t creation_{};
  unsigned index_{};
};
} // namespace mhw::bridge_probe

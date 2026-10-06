#pragma once
#include <windows.h>
#include <d3d12.h>
#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace mhw::projection_binding::depth_readback {
inline constexpr std::uint64_t byte_limit=64ULL*1024*1024;
enum class Phase : unsigned { disabled,ready,issuing,recorded,signaling,submitted,complete,saved,failed,quarantined };
struct Site {
  bool request_control{};
  std::uint64_t marker{},reset{},barrier{},copy{},copy_exit{},begin{},end{},close{},execute{},execute_exit{},signal_begin{},signal_end{},selection{};
  std::uintptr_t resource{},identity{},command{},allocator{},queue{};
};
// One attempt per capture slot. After issue, resources deliberately remain pinned
// until process exit, including success: the game may resubmit a closed list.
class Capture {
 public:
  bool prepare(ID3D12Device *,unsigned,unsigned) noexcept;
  bool issue(ID3D12GraphicsCommandList *,ID3D12Resource *,const Site &) noexcept;
  bool submit(ID3D12CommandQueue *,const Site &) noexcept;
  void poll() noexcept;
  void abort() noexcept;
  bool save(const std::wstring &);
  void write(std::ostream &) const;
  Phase phase() const noexcept {return phase_.load(std::memory_order_acquire);}
  bool terminal() const noexcept;
  bool issued() const noexcept {return attempted_.load(std::memory_order_acquire);}
  ID3D12Fence *fence() const noexcept {return fence_;}
  const Site &site() const noexcept {return record_site_;} // immutable after recorded publication
  const Site &submitted_site() const noexcept {return site_;} // after submitted acquire, immutable until worker-only retirement
  void retire(std::uint64_t begin,std::uint64_t end) noexcept {retire_reset_=begin;retire_exit_=end;retired_=true;}
  void issue_end(std::uint64_t order) noexcept {site_.end=order;record_site_=site_;if(phase()==Phase::issuing)phase_.store(Phase::recorded,std::memory_order_release);}
  void signal_end(std::uint64_t order) noexcept {site_.signal_end=order;if(phase()==Phase::signaling)phase_.store(Phase::submitted,std::memory_order_release);}
  bool source_matches(const D3D12_RESOURCE_DESC &) const noexcept;
  static bool extent(unsigned,unsigned) noexcept;
#ifdef MHW_BINDING_FIXTURE
  bool fixture_release_after_reset(bool) noexcept;
  void fixture_signal_failure() noexcept {signal_failure_=true;}
  void fixture_map_failure() noexcept {map_failure_=true;}
#endif
 private:
  bool issue_impl(ID3D12GraphicsCommandList *,ID3D12Resource *,const Site &);
  bool submit_impl(ID3D12CommandQueue *,const Site &);
  void release_unissued() noexcept;
  // Raw owning references prevent automatic Release of possibly in-flight
  // resources during worker unwind. No production release exists after issue.
  ID3D12Device *device_{};
  ID3D12Resource *buffer_{},*source_{};
  ID3D12GraphicsCommandList *list_{};
  ID3D12CommandAllocator *allocator_{};
  ID3D12CommandQueue *queue_{};
  ID3D12Fence *fence_{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
  std::uint64_t bytes_{},row_bytes_{},completed_{},poll_tick_{},pixel_bytes_{};
  std::uint64_t retire_reset_{},retire_exit_{};
  unsigned rows_{},width_{},height_{},signal_attempts_{},signal_result_{0x8000000a},map_result_{0x8000000a};
  Site site_{},record_site_{};
  std::atomic<Phase> phase_{Phase::disabled};
  std::atomic<bool> attempted_{};
  bool copy_returned_{},retained_{},mapped_{};
  std::atomic<bool> retired_{};
  std::uint64_t finite_{},nonfinite_{},zero_{},one_{},outside_{};
  double minimum_{},maximum_{};
#ifdef MHW_BINDING_FIXTURE
  bool signal_failure_{},map_failure_{};
#endif
};
}

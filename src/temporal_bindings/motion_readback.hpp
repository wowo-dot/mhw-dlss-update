#pragma once
#include <windows.h>
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <iosfwd>
namespace mhw::temporal_bindings::motion {
inline constexpr std::uint64_t byte_limit=16ULL*1024*1024;
enum class Kind : unsigned {motion,color,depth};
using BarrierFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_RESOURCE_BARRIER *);
struct Site {
  unsigned receipt{};
  std::uint64_t frame{},reset{},marker{},begin{},end{};
  std::uintptr_t packet{},command{},allocator{},resource{},identity{};
};
struct Completion {
  Site site;
  std::uint64_t forward{},dispatch_exit{},close{},execute{},execute_exit{},signal{},signal_exit{},completed{},retire{},retire_exit{};
  std::uintptr_t queue{},fence{};
};
enum class Phase : unsigned {disabled,ready,recorded,qualified,saved,failed,quarantined};
// Worker prepares/saves; one admitted native callback issues. The core drains
// callbacks before qualification. Issued references never release in production.
class Capture {
 public:
  bool prepare(ID3D12Device *,unsigned,unsigned,Kind=Kind::motion,ID3D12Resource *owned=nullptr) noexcept;
  bool source_matches(ID3D12Resource *) const noexcept;
  bool issue(ID3D12GraphicsCommandList *,ID3D12Resource *,BarrierFn,const Site &) noexcept;
  bool issue_depth_draw(ID3D12GraphicsCommandList *,ID3D12Resource *,BarrierFn,const Site &) noexcept;
  void issue_end(std::uint64_t) noexcept;
  bool qualify(const Completion &) noexcept;
  bool save(const std::wstring &);
  void abort() noexcept;
  void write(std::ostream &) const;
  bool issued() const noexcept {return attempted_;}
  Phase phase() const noexcept {return phase_;}
  static bool extent(unsigned,unsigned) noexcept;
  bool source_matches(const D3D12_RESOURCE_DESC &) const noexcept;
  bool owned_ready(ID3D12Resource *r) const noexcept {
    return r && r==owned_ && owned_copied_ && restored_ && phase_==Phase::saved;
  }
#ifdef MHW_TEMPORAL_FIXTURE
  bool fixture_release_after_reset(bool) noexcept;
  void fixture_map_failure() noexcept {map_failure_=true;}
#endif
 private:
  bool issue_impl(ID3D12GraphicsCommandList *,ID3D12Resource *,BarrierFn,const Site &);
  bool commands(ID3D12GraphicsCommandList *,BarrierFn) noexcept;
  void release_unissued() noexcept;
  ID3D12Device *device_{};
  ID3D12Resource *buffer_{},*source_{},*owned_{};
  ID3D12GraphicsCommandList *list_{};
  ID3D12CommandAllocator *allocator_{};
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
  std::uint64_t bytes_{},row_bytes_{},pixel_bytes_{};
  unsigned rows_{},width_{},height_{},map_result_{0x8000000a};
  DXGI_FORMAT format_{DXGI_FORMAT_R32_UINT};
  D3D12_RESOURCE_FLAGS flags_{D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};
  D3D12_RESOURCE_STATES state_{static_cast<D3D12_RESOURCE_STATES>(0xc0)};
  Site site_{};Completion completion_{};
  Phase phase_{Phase::disabled};
  bool attempted_{},transitioned_{},copied_{},restored_{},retained_{},mapped_{};
  bool owned_copied_{};
  std::string file_,hash_;
#ifdef MHW_TEMPORAL_FIXTURE
  bool map_failure_{};
#endif
};
}

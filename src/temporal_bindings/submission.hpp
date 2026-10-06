#pragma once
#include "inputs.hpp"
#include "motion_readback.hpp"
#include <d3d12.h>
#include <iosfwd>
namespace mhw::temporal_bindings::submission {
inline constexpr unsigned event_limit=4096, list_limit=128, allocator_limit=128;
inline constexpr unsigned queue_limit=8, batch_limit=128, submitted_limit=8192, warmup_ms=150;
bool install(const Config &,std::string &);
void begin(std::uint64_t) noexcept;
bool active() noexcept;
bool warmed() noexcept;
struct RecordingStamp {std::uint64_t reset{},enter{};std::uintptr_t command{},allocator{};};
bool mark(unsigned,std::uint64_t,std::uintptr_t,ID3D12GraphicsCommandList *,RecordingStamp *stamp=nullptr) noexcept;
// Read-only native-copy marker, sharing the Reset/Close/Execute order domain.
enum class MarkerFailure : unsigned {none,not_configured,admission_closed,unknown_list,missing_reset,list_closed,list_busy,missing_allocator,event_budget_or_deadline};
unsigned native_copy_begin(ID3D12GraphicsCommandList *,MarkerFailure &) noexcept;
// A downstream command must belong to the exact still-open temporal recording.
// The native_copy marker excludes concurrent Reset/Close/Execute until end.
unsigned followup_begin(unsigned receipt,ID3D12GraphicsCommandList *,std::uint64_t generation) noexcept;
void native_copy_end(unsigned,bool) noexcept;
// Selected ResourceBarrier observations share the same ordering and recording
// exclusion as Reset/Close/Execute, without assuming GPU order between lists.
unsigned barrier_begin(ID3D12GraphicsCommandList *,MarkerFailure &,RecordingStamp &) noexcept;
void barrier_end(unsigned,bool) noexcept;
void enable_depth_copies() noexcept;
bool depth_begin(unsigned,unsigned,std::uint64_t,std::uintptr_t,std::uintptr_t,std::uintptr_t,motion::Site &) noexcept;
std::uint64_t depth_end(unsigned,bool) noexcept;
bool depth_completion(unsigned,motion::Completion &) noexcept;
void mark_end(unsigned,bool) noexcept;
void enable_copies(bool colors=false) noexcept;
bool copy_begin(unsigned,std::uintptr_t,std::uintptr_t,motion::Site &,bool color=false) noexcept;
std::uint64_t copy_end(unsigned,bool,bool color=false) noexcept;
void dispatch_forward(unsigned) noexcept;
bool inline_begin(unsigned,std::uint64_t,ID3D12GraphicsCommandList *,std::uint64_t generation) noexcept;
void inline_end(unsigned,bool) noexcept;
bool copy_completion(unsigned,motion::Completion &,bool color=false) noexcept;
void stop() noexcept;
bool admission_open() noexcept;
std::uint64_t cutoff_tick() noexcept;
unsigned recorded_events() noexcept;
void poll() noexcept;
bool healthy() noexcept;
bool complete() noexcept;
// Exact selected recording has completed on the GPU AND retired. Optional
// fence is AddRef'd for a caller that needs to preserve this proof at closure.
bool completed_receipt(unsigned,ID3D12Fence **held_fence=nullptr) noexcept;
bool settled() noexcept;
bool finish(ID3D12Fence **completed_fence=nullptr,unsigned receipt=0) noexcept;
bool disabled() noexcept;
void write(std::ostream &);
#ifdef MHW_TEMPORAL_FIXTURE
bool fixture_start(const Config &);
HRESULT fixture_reset(ID3D12GraphicsCommandList *,ID3D12CommandAllocator *);
HRESULT fixture_close(ID3D12GraphicsCommandList *);
void fixture_execute(ID3D12CommandQueue *,unsigned,ID3D12CommandList *const *);
void fixture_signal_failure() noexcept;
void fixture_before_close_return(void (*)()) noexcept;
#endif
}

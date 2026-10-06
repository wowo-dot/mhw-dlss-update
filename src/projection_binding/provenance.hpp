#pragma once
#include "recorder.hpp"
#include <atomic>
#include <climits>
namespace mhw::projection_binding::provenance {
// Five request/readback samples include the pre-selection state history.
// Keep this fixed and append-only; overflow still closes provenance.
inline constexpr unsigned watch_capacity=2048, history_capacity=8192, barrier_capacity=65536;
inline constexpr unsigned range_capacity=32,pin_capacity=256,watch_ms=1800000;
enum Kind : unsigned { construct,destroy,create_dsv,copy_descriptors,copy_simple,close,clear_depth,bundle,
  list_reset,resource_barrier,execute,queue_signal,queue_wait,fence_completed,fence_signal,copy_texture,copy_resource };
// Physical hook 8 is ExecuteCommandLists; Reset/ResourceBarrier share core hooks.
inline constexpr unsigned own_count=17, list_capacity=128, queue_capacity=8, fence_capacity=32;
inline constexpr unsigned submission_capacity=4096, batch_capacity=128;
enum class Error : unsigned { none,capacity,read,identity,changed,exception,reentrant,target,deadline,install,device_removed };
struct Range {std::uintptr_t start{};unsigned count{};};
struct Event {
  unsigned kind{},thread{},parent{},count{},first{},result{};
  std::uint64_t enter{},exit{},tick{};
  std::uintptr_t object{},caller{},resource{},identity{},handle{},backing{};
  bool returned{},valid{},null_desc{};
  std::array<unsigned,6> descriptor{};
  std::array<unsigned,5> recipe{};
  std::array<Range,range_capacity> ranges{};
};
struct Slot {std::atomic<unsigned> published{};Event value{};std::atomic<std::uintptr_t> pin{};};
struct Marker {
  std::uint64_t order{},bind_enter{},bind_exit{};
  std::uintptr_t command{},view{},resource{},identity{},handle{};
};
struct Config {
  std::uintptr_t image{},backend{},device{},heap{},heap_start{};
  unsigned heap_count{},stride{};
  std::uintptr_t frame_renderer{}; // Enables the two bounded packet receipt hooks.
  bool same_frame_readback{},triplet_readback{},sequence_readback{};
  std::array<std::uintptr_t,own_count> targets{};
};
struct Token {unsigned index{UINT_MAX},kind{};bool admitted{},history{},constructor{};unsigned prior_parent{};};
enum class Start { waiting,active,failed };
Start start_watch(std::uintptr_t,std::string &);
void poll_watch() noexcept;
bool start_frame(const Runtime &) noexcept;
bool select_source(const Source &,unsigned stage=1) noexcept;
void end_frame() noexcept;
bool awaiting_submission() noexcept;
void stop() noexcept;
bool settled() noexcept;
bool disable() noexcept;
bool healthy() noexcept;
bool active() noexcept;
Token history_before(unsigned,const Frame &) noexcept;
void after(Token,const Frame &,bool) noexcept;
void mark_draw(const Draw &) noexcept;
void mark_binding(bool returned) noexcept;
void dispatch(unsigned,Frame *);
void write_ready(std::ostream &);
void write_result(std::ostream &);
bool prepare_readback(const Runtime &) noexcept;
bool save_readback(const std::wstring &,unsigned stage=1);
void write_readback(std::ostream &,unsigned stage=1);
// Pure argument decoding: no native code, allocation, or GPU work.
bool destination_ranges(unsigned count,const std::uintptr_t *starts,const unsigned *sizes,unsigned stride,
                        std::array<Range,range_capacity> &) noexcept;
bool valid_depth_view(const std::array<unsigned,6> &,const ResourceDescription &,unsigned &) noexcept;
#ifdef MHW_BINDING_FIXTURE
bool install_owned_watch(const Config &) noexcept;
bool remove_owned_watch() noexcept;
Error owned_error() noexcept;
unsigned owned_pins() noexcept;
unsigned owned_draw_return_calls() noexcept;
const Marker &owned_marker() noexcept;
void owned_expire() noexcept;
Token owned_execute_before(const Frame &) noexcept;
Token owned_copy_before(unsigned,const Frame &) noexcept;
void owned_pause_copy_pin() noexcept;
bool owned_copy_pin_waiting() noexcept;
void owned_resume_copy_pin() noexcept;
void owned_change_copy_pin(unsigned) noexcept;
bool owned_release_readback(bool) noexcept;
#endif
}

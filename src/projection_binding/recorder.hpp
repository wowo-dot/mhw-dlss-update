#pragma once
#include "projection_snapshot.hpp"
#include "attachments.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <iosfwd>

namespace mhw::projection_binding {
inline constexpr char host_sha[]="c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea";
inline constexpr unsigned window_ms=2000, max_constants=4096, max_shader=256*1024;
inline constexpr unsigned max_api=128, max_draws=16, max_blobs=5;
inline constexpr unsigned max_recovery_deferrals=64, max_draw_waits=64, sequence_samples=5;
inline constexpr std::array<unsigned,10> native_rvas={0x228f3b0,0x23d2a80,0x259e1e0,0x259e670,
  0x259d670,0x259d740,0x259d820,0x259d930,0x259da10,0x23a2b50};
enum Kind : unsigned { producer,publish,consume,state_setup,draw0,draw1,draw2,draw3,draw4,
    root_cbv,root_table,root_signature,pipeline,viewport,api_draw,api_indexed,api_indirect,create_cbv,reset,rollover,
    output_binding,barriers,create_pipeline,scissor,kind_count };
enum class Stop : unsigned { idle, recording, complete, requested, deadline, invalid, capacity };
enum class Phase : unsigned { empty, source, published, consumed, drawn };
enum class Fault : unsigned { none, read, identity, layout, arena, changed, overflow, contention, exception, reentrant, api_target, shader };
// The ASM wrapper preserves integer/vector arguments and return registers.
// EFLAGS are volatile in this ABI. Only actual stack arguments are read.
struct Frame {
  std::array<std::uintptr_t,7> gpr{}; // RAX RCX RDX R8 R9 R10 R11
  std::uint64_t flags{};
  std::array<std::byte,96> xmm{};
  std::uintptr_t caller{};
  std::array<std::uintptr_t,4> stack{};
};
static_assert(offsetof(Frame,caller)==0xa0 && offsetof(Frame,stack)==0xa8 && sizeof(Frame)==0xc8);
struct Arena {
  std::uintptr_t backend{}, cpu_resource{}, gpu_resource{};
  unsigned mode{}, index{}, stride{};
  std::array<std::uintptr_t,3> cpu{},gpu{};
  std::array<unsigned,3> used{};
  std::uintptr_t delta{};
};
struct Location { bool valid{}; unsigned arena{}, offset{}, aligned_bytes{}; std::uint64_t gpu{}; };
Location locate(const Arena &,std::uintptr_t cpu,unsigned bytes) noexcept;
bool dxbc_valid(const std::byte *,unsigned size) noexcept;
inline constexpr std::array<unsigned,3> matrix_id_rvas={0x4be75f8,0x4be75fc,0x4be7640};
struct MatrixSet {
  bool valid{};
  std::array<unsigned,3> ids{}; // fViewProj, fView, fNoJitterViewProj
  std::array<std::array<unsigned,16>,3> bits{};
};
bool matrix_layout(const std::array<unsigned,5> &ids,unsigned words) noexcept;
struct Source {
  bool valid{};
  unsigned bytes{}, thread{};
  std::uint64_t order{},tick{};
  std::uintptr_t camera{}, camera_type{};
  bridge_probe::ProjectionAnchor anchor{};
  bridge_probe::ProjectionSnapshot projection{};
  Arena arena{}; Location location{};
  MatrixSet matrices{};
  std::array<std::byte,max_constants> data{};
};
bool capture_matrices(std::uintptr_t,Source &) noexcept;
bool matrix_ids_same(std::uintptr_t,const Source &) noexcept;
struct RootSelection {
  bool valid{};
  unsigned index{},ps_flags{},vs_flags{},ps_count{},descriptor_mode{};
  std::uintptr_t recipe{},recipe_device{};
  std::array<unsigned char,3> recipe_flags{}; // serializer version, buffer mode, descriptor mode
  std::array<std::uintptr_t,5> signatures{}; // backend +278,+280,+288,+290,+298
  bool operator==(const RootSelection &) const = default;
};
unsigned root_variant(unsigned ps_flags,unsigned vs_flags,unsigned ps_count,bool gs,bool hs) noexcept;
bool capture_root(std::uintptr_t backend,std::uintptr_t set,const std::array<std::uintptr_t,5> &,RootSelection &) noexcept;
struct Publication {
  bool valid{}; unsigned index{}, count{}, thread{};
  std::uint64_t order{}, tick{};
  std::uintptr_t packet{},context{},pointer{}; unsigned id{};
};
struct Consumption {
  bool valid{}; unsigned thread{};
  std::uint64_t order{},tick{};
  std::uintptr_t packet{},context{},executor{},pointer{};
  Arena arena{}; Location location{};
};
struct Api {
  unsigned kind{},draw{},thread{},size{};
  std::uint64_t order{},exit_order{},tick{};
  std::uintptr_t object{},caller{};
  std::array<std::uint64_t,7> args{};
  std::array<std::uint32_t,6> viewport{};
  bool returned{};
  OutputBinding outputs{};
  unsigned barrier_count{};
  std::array<BarrierRecord,max_barriers> barrier_records{};
  std::array<int,4> scissor{};
};
struct Blob {
  unsigned size{};
  std::array<std::byte,max_shader> bytes{};
};
struct Draw {
  unsigned kind{},thread{},binding_mode{},rejection{};
  bool returned{},state_seen{},source_unchanged{},root_binding_observed{},descriptor_observed{},draw_api_observed{};
  std::uint64_t order{},exit_order{},tick{},gpu{};
  std::uintptr_t executor{},context{},packet{},graphics{},shader_set{},command{},pso{},root{},device{};
  std::uint64_t heap_cpu{},heap_gpu{}; unsigned descriptor_stride{};
  std::array<unsigned,5> blobs{}; // one-based index, zero means absent
  std::array<std::uintptr_t,5> stage_entries{};
  RootSelection selection{};
  bool root_stable{};
  AttachmentCapture attachments{};
  PipelineCreation creation{};
};
struct Provider {
  bool valid{}; unsigned use{},sizing{};
  std::uintptr_t backend{},device{},wrapper{};
  bool operator==(const Provider &) const = default;
};
struct JitterCall {
  Source source{};
  Provider before{},after{};
  std::uint64_t candidate{},restored_bits{},exit_order{};
  bool injected{},restored{},returned{},complete{};
};
struct Stimulus {
  bool enabled{},sequence{}; unsigned count{},injections{},restorations{},recovery_deferrals{},draw_waits{};
  std::array<JitterCall,sequence_samples> calls{};
};
bool jitter_exchange(std::uintptr_t,std::uint64_t,std::uint64_t) noexcept;
bool jitter_finite(std::uint64_t) noexcept;
struct Capture {
  bool attachments_enabled{};
  Phase phase{}; Fault fault{};
  Source source{}; Publication publication{}; Consumption consumption{};
  Stimulus stimulus{};
  unsigned draw_count{},api_count{},blob_count{},drops{};
  std::array<Draw,max_draws> draws{};
  std::array<Api,max_api> apis{};
  std::array<Blob,max_blobs> blobs{};
};
struct Runtime {
  std::uintptr_t backend{},device{},swapchain{},cpu_resource{},gpu_resource{};
  // width/height: the game's scene size (renderer display). It differs from
  // the swap chain (window_*) only with the aspect-ratio option, which draws
  // the scene letterboxed or pillarboxed in the window (scene_override 1).
  unsigned mode{},format{},width{},height{},buffers{},luid_low{};
  unsigned window_width{},window_height{},scene_override{};
  int luid_high{};
  std::uint64_t cpu_gpu_base{},gpu_base{},cpu_bytes{},gpu_bytes{};
  std::array<std::uintptr_t,kind_count> targets{};
};
inline constexpr std::array<unsigned,kind_count> method_slots={0,0,0,0,0,0,0,0,0,38,32,30,25,21,12,13,59,17,10,0,46,26,10,22};
template<class T> bool read(std::uintptr_t,T &) noexcept;
bool copy_memory(std::uintptr_t,void *,unsigned) noexcept;
bool read_arena(std::uintptr_t,Arena &) noexcept;
struct Status {
  Stop stop{}; Phase phase{}; Fault fault{}; unsigned pending{},visits{},drops{}; bool disabled{};
  unsigned contention_site{},contention_thread{},owner_site{},owner_thread{};
};
struct Install { unsigned stage{},kind{}; int status{}; unsigned win32{}; };
struct PoolState {
  bool attempted{},ready{},policy_available{};
  unsigned policy_flags{},win32{}; int status{};
  std::uintptr_t origin{},slot{},page{};
};
const PoolState &pool_state() noexcept;
bool reserve_startup(std::uintptr_t base,std::string &reason);
bool reserve_pool(const std::array<std::uintptr_t,10> &,std::string &reason);
std::string file_sha256(const std::wstring &);
std::string bytes_sha256(const void *,unsigned);
// hash_file=false: the caller verified the exe file hash another way (AL).
bool host_identity(std::uintptr_t,std::string &,bool hash_file=true);
bool verify_code(std::uintptr_t,unsigned,unsigned,const char *);
// The same hash with the first head_size bytes taken from `head` (their original
// values, where a shared hook chain owns the function's entry) and the rest live.
bool verify_code_with_head(std::uintptr_t,unsigned,unsigned,const char *,const std::uint8_t *head,unsigned head_size);
bool prepare(std::uintptr_t,std::string &); // Gates code, pins COM discovery; no game data writes.
bool arm(std::string &);
void poll() noexcept;
void stop() noexcept;
bool settled() noexcept;
bool disable() noexcept; // Retains trampolines and pinned DLL for in-flight forwarding.
Status status() noexcept;
Install install_status() noexcept;
const Runtime &runtime() noexcept;
const Capture *finished_capture() noexcept;
void write_capture(std::ostream &,const Capture &,const std::wstring &output_prefix);
#ifdef MHW_BINDING_FIXTURE
bool install_owned(std::uintptr_t,const Runtime &,const std::array<std::uintptr_t,kind_count> &,bool stimulus=false,bool attachments=false,bool sequence=false) noexcept;
bool remove_owned() noexcept;
void expire_owned() noexcept;
bool hold_owned(unsigned kind,bool exiting) noexcept;
void release_owned() noexcept;
#endif
}

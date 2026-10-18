#pragma once
#include "projection_snapshot.hpp"
#include "attachments.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mhw::projection_binding {
inline constexpr char host_sha[] = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea";
inline constexpr unsigned max_constants = 4096, max_shader = 256 * 1024;
inline constexpr std::array<unsigned, 10> native_rvas = {0x228f3b0, 0x23d2a80, 0x259e1e0, 0x259e670, 0x259d670,
                                                         0x259d740, 0x259d820, 0x259d930, 0x259da10, 0x23a2b50};
// The game functions and D3D12 methods discover() resolves into Runtime::targets.
enum Kind : unsigned {
    producer,
    publish,
    consume,
    state_setup,
    draw0,
    draw1,
    draw2,
    draw3,
    draw4,
    root_cbv,
    root_table,
    root_signature,
    pipeline,
    viewport,
    api_draw,
    api_indexed,
    api_indirect,
    create_cbv,
    reset,
    rollover,
    output_binding,
    barriers,
    create_pipeline,
    scissor,
    kind_count
};
// The ASM wrapper preserves integer/vector arguments and return registers.
// EFLAGS are volatile in this ABI. Only actual stack arguments are read.
struct Frame {
    std::array<std::uintptr_t, 7> gpr{}; // RAX RCX RDX R8 R9 R10 R11
    std::uint64_t flags{};
    std::array<std::byte, 96> xmm{};
    std::uintptr_t caller{};
    std::array<std::uintptr_t, 4> stack{};
};
static_assert(offsetof(Frame, caller) == 0xa0 && offsetof(Frame, stack) == 0xa8 && sizeof(Frame) == 0xc8);
struct Arena {
    std::uintptr_t backend{}, cpu_resource{}, gpu_resource{};
    unsigned mode{}, index{}, stride{};
    std::array<std::uintptr_t, 3> cpu{}, gpu{};
    std::array<unsigned, 3> used{};
    std::uintptr_t delta{};
};
bool dxbc_valid(const std::byte *, unsigned size) noexcept;
inline constexpr std::array<unsigned, 3> matrix_id_rvas = {0x4be75f8, 0x4be75fc, 0x4be7640};
struct MatrixSet {
    bool valid{};
    std::array<unsigned, 3> ids{}; // fViewProj, fView, fNoJitterViewProj
    std::array<std::array<unsigned, 16>, 3> bits{};
};
bool matrix_layout(const std::array<unsigned, 5> &ids, unsigned words) noexcept;
struct Source {
    bool valid{};
    unsigned bytes{}, thread{};
    std::uint64_t order{}, tick{};
    std::uintptr_t camera{}, camera_type{};
    game_memory::ProjectionAnchor anchor{};
    game_memory::ProjectionSnapshot projection{};
    MatrixSet matrices{};
    std::array<std::byte, max_constants> data{};
};
bool capture_matrices(std::uintptr_t, Source &) noexcept;
bool matrix_ids_same(std::uintptr_t, const Source &) noexcept;
struct Provider {
    bool valid{};
    unsigned use{}, sizing{};
    std::uintptr_t backend{}, device{}, wrapper{};
    bool operator==(const Provider &) const = default;
};
bool jitter_exchange(std::uintptr_t, std::uint64_t, std::uint64_t) noexcept;
bool jitter_finite(std::uint64_t) noexcept;
struct Runtime {
    std::uintptr_t backend{}, device{}, swapchain{}, cpu_resource{}, gpu_resource{};
    // width/height: the game's scene size (renderer display). It differs from
    // the swap chain (window_*) only with the aspect-ratio option, which draws
    // the scene letterboxed or pillarboxed in the window (scene_override 1).
    unsigned mode{}, format{}, width{}, height{}, buffers{}, luid_low{};
    unsigned window_width{}, window_height{}, scene_override{};
    int luid_high{};
    std::uint64_t cpu_gpu_base{}, gpu_base{}, cpu_bytes{}, gpu_bytes{};
    std::array<std::uintptr_t, kind_count> targets{};
};
inline constexpr std::array<unsigned, kind_count> method_slots = {0,  0,  0,  0,  0,  0,  0,  0, 0,  38, 32, 30,
                                                                  25, 21, 12, 13, 59, 17, 10, 0, 46, 26, 10, 22};
template <class T> bool read(std::uintptr_t, T &) noexcept;
bool copy_memory(std::uintptr_t, void *, unsigned) noexcept;
bool read_arena(std::uintptr_t, Arena &) noexcept;
std::string file_sha256(const std::wstring &);
// A file's hash with the reason it could not be taken (v0.28.5): the stage
// that stopped ("open", "size", "read", "short", "hash"; empty when complete),
// its GetLastError and how far it got.
struct FileDigest {
    std::string sha;
    const char *stage = "";
    unsigned long error = 0;
    std::uint64_t size = 0, read = 0;
};
FileDigest file_digest(const std::wstring &); // CreateFileW and ReadFile
std::string bytes_sha256(const void *, unsigned);
// hash_file=false: the caller verified the exe file hash another way (AL).
bool host_identity(std::uintptr_t, std::string &, bool hash_file = true);
bool verify_code(std::uintptr_t, unsigned, unsigned, const char *);
// The same hash with the first head_size bytes taken from `head` (their original
// values, where a shared hook chain owns the function's entry) and the rest live.
bool verify_code_with_head(std::uintptr_t, unsigned, unsigned, const char *, const std::uint8_t *head,
                           unsigned head_size);
} // namespace mhw::projection_binding

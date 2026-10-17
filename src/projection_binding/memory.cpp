#include "memory.hpp"
#include "../bridge_probe/readable_region.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>

namespace mhw::projection_binding {
bool copy_memory(std::uintptr_t at, void *out, unsigned bytes) noexcept {
    if (!at || !out || !bytes || bytes > max_shader || at > UINTPTR_MAX - bytes) return false;
    for (unsigned done = 0; done < bytes;) {
        const auto p = at + done, end = bridge_probe::readable_end(p);
        if (!end) return false;
        done += static_cast<unsigned>((std::min)(static_cast<std::uintptr_t>(bytes - done), end - p));
    }
    __try {
        std::memcpy(out, reinterpret_cast<const void *>(at), bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// The game's per-frame constant arena (sRender backend +0xff0..+0x10a0).
bool read_arena(std::uintptr_t b, Arena &a) noexcept {
    a.backend = b;
    if (!b || b > UINTPTR_MAX - 0x10a0 || !read(b + 0xff0, a.mode) || !read(b + 0x1000, a.stride) ||
        !read(b + 0x103c, a.index) || !read(b + 0x1058, a.cpu_resource) || !read(b + 0x1098, a.delta))
        return false;
    if (a.mode == 2)
        a.gpu_resource = a.cpu_resource;
    else if (!read(b + 0x1078, a.gpu_resource))
        return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!read(b + 0x1018 + 8 * i, a.cpu[i]) || !read(b + 0x1080 + 8 * i, a.gpu[i]) ||
            !read(b + 0x1030 + 4 * i, a.used[i]))
            return false;
    return a.cpu_resource && a.gpu_resource && a.mode <= 2;
}
} // namespace mhw::projection_binding

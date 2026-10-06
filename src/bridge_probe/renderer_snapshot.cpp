#include "renderer_snapshot.hpp"
#include <cmath>
#include <limits>

namespace mhw::bridge_probe {
namespace {
struct CaptureReader {
  RendererSnapshot &snapshot;
  ScalarReader reader;
  void *context;
  template <typename T>
  bool get(std::uintptr_t base, std::uintptr_t offset, T &value,
           RendererReadField field) noexcept {
    if (!reader || snapshot.reads >= kRendererSnapshotReadBudget ||
        base == 0U ||
        base > std::numeric_limits<std::uintptr_t>::max() - offset) {
      snapshot.read_result = ScalarReadResult::unreadable;
    } else {
      ++snapshot.reads;
      snapshot.read_result =
          reader(context, base + offset,
                 std::as_writable_bytes(std::span<T>(&value, 1U)));
    }
    if (snapshot.read_result == ScalarReadResult::success)
      return true;
    snapshot.status = RendererSnapshotStatus::read_failed;
    snapshot.failed_field = field;
    return false;
  }
};
} // namespace

RendererSnapshot capture_renderer_snapshot(std::uintptr_t singleton,
                                           ScalarReader reader,
                                           void *context) noexcept {
  RendererSnapshot s{};
  CaptureReader read{s, reader, context};
  std::uintptr_t root{}, backend{}, view{}, ngx{};
  if (!read.get(singleton, 0U, root, RendererReadField::singleton))
    return s;
  if (!root)
    return s;
  s.renderer_present = true;
  if (!read.get(root, 120U, backend, RendererReadField::backend) ||
      !read.get(root, 165U, s.use_dlss, RendererReadField::use_dlss) ||
      !read.get(root, 180U, s.sizing_flags, RendererReadField::sizing_flags) ||
      !read.get(root, 176U, s.optimal_scale,
                RendererReadField::optimal_scale) ||
      !read.get(root, 408U, s.present_width,
                RendererReadField::present_width) ||
      !read.get(root, 412U, s.present_height,
                RendererReadField::present_height) ||
      !read.get(root, 516U, s.content_scale,
                RendererReadField::content_scale) ||
      !read.get(root, 127504U, s.frame_index, RendererReadField::frame_index) ||
      !read.get(root, 128080U, view, RendererReadField::main_view))
    return s;
  s.backend_present = backend != 0U;
  s.main_view_present = view != 0U;
  if (view &&
      !read.get(view, 88U, s.main_view_flags, RendererReadField::view_flags))
    return s;
  if (backend) {
    if (!read.get(backend, 272152U, s.backend_ready,
                  RendererReadField::backend_ready) ||
        !read.get(backend, 272128U, ngx, RendererReadField::ngx_context))
      return s;
    s.ngx_context_present = ngx != 0U;
  }
  // Avoid claiming a stable graph across obvious recreation. Never retry reads
  // within this snapshot and never dereference the borrowed NGX context.
  std::uintptr_t root_after{}, backend_after{}, view_after{};
  if (!read.get(singleton, 0U, root_after,
                RendererReadField::singleton_recheck))
    return s;
  if (root_after != root) {
    s.status = RendererSnapshotStatus::pointers_changed;
    return s;
  }
  if (!read.get(root, 120U, backend_after,
                RendererReadField::backend_recheck) ||
      !read.get(root, 128080U, view_after, RendererReadField::view_recheck))
    return s;
  s.pointers_stable = backend_after == backend && view_after == view;
  if (!s.pointers_stable) {
    s.status = RendererSnapshotStatus::pointers_changed;
    return s;
  }
  if (s.use_dlss > 1U || s.backend_ready > 1U || s.present_width == 0U ||
      s.present_width > 16384U || s.present_height == 0U ||
      s.present_height > 16384U || !std::isfinite(s.optimal_scale) ||
      s.optimal_scale <= 0.0F || s.optimal_scale > 4.0F ||
      !std::isfinite(s.content_scale) || s.content_scale <= 0.0F ||
      s.content_scale > 4.0F) {
    s.status = RendererSnapshotStatus::invalid_values;
    return s;
  }
  if (!s.use_dlss)
    s.block_mask |= RendererBlock::use_dlss_off;
  if (!(s.sizing_flags & 1U))
    s.block_mask |= RendererBlock::sizing_disallows_dlss;
  if (!s.backend_present)
    s.block_mask |= RendererBlock::backend_absent;
  if (!s.backend_ready)
    s.block_mask |= RendererBlock::backend_not_ready;
  if (!s.ngx_context_present)
    s.block_mask |= RendererBlock::ngx_context_absent;
  s.status = s.block_mask == 0U ? RendererSnapshotStatus::gates_ready
                                : RendererSnapshotStatus::gates_blocked;
  return s;
}

std::string_view renderer_status_name(RendererSnapshotStatus value) noexcept {
  switch (value) {
  case RendererSnapshotStatus::renderer_absent:
    return "renderer-absent";
  case RendererSnapshotStatus::read_failed:
    return "read-failed";
  case RendererSnapshotStatus::invalid_values:
    return "invalid-values";
  case RendererSnapshotStatus::pointers_changed:
    return "pointers-changed";
  case RendererSnapshotStatus::gates_blocked:
    return "gates-blocked";
  case RendererSnapshotStatus::gates_ready:
    return "gates-ready";
  }
  return "unknown";
}
std::string_view renderer_field_name(RendererReadField value) noexcept {
  switch (value) {
  case RendererReadField::none:
    return "none";
  case RendererReadField::singleton:
    return "singleton";
  case RendererReadField::backend:
    return "backend";
  case RendererReadField::use_dlss:
    return "use-dlss";
  case RendererReadField::sizing_flags:
    return "sizing-flags";
  case RendererReadField::optimal_scale:
    return "optimal-scale";
  case RendererReadField::present_width:
    return "present-width";
  case RendererReadField::present_height:
    return "present-height";
  case RendererReadField::content_scale:
    return "content-scale";
  case RendererReadField::frame_index:
    return "frame-index";
  case RendererReadField::main_view:
    return "main-view";
  case RendererReadField::view_flags:
    return "view-flags";
  case RendererReadField::backend_ready:
    return "backend-ready";
  case RendererReadField::ngx_context:
    return "ngx-context";
  case RendererReadField::singleton_recheck:
    return "singleton-recheck";
  case RendererReadField::backend_recheck:
    return "backend-recheck";
  case RendererReadField::view_recheck:
    return "view-recheck";
  }
  return "unknown";
}
bool same_renderer_gate_values(const RendererSnapshot &a,
                               const RendererSnapshot &b) noexcept {
  return a.status == b.status && a.failed_field == b.failed_field &&
         a.read_result == b.read_result &&
         a.renderer_present == b.renderer_present &&
         a.backend_present == b.backend_present &&
         a.ngx_context_present == b.ngx_context_present &&
         a.main_view_present == b.main_view_present &&
         a.pointers_stable == b.pointers_stable && a.use_dlss == b.use_dlss &&
         a.backend_ready == b.backend_ready &&
         a.sizing_flags == b.sizing_flags &&
         a.present_width == b.present_width &&
         a.present_height == b.present_height &&
         a.main_view_flags == b.main_view_flags &&
         a.optimal_scale == b.optimal_scale &&
         a.content_scale == b.content_scale && a.block_mask == b.block_mask;
}
} // namespace mhw::bridge_probe

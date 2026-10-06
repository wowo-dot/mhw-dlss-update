#pragma once
#include "depth_record.hpp"
namespace mhw::exception_capture {
struct TrackerSnapshot {
  Snapshot data;
  bool complete{}, same_epoch{}, compatible_read{};
};
// Called only during the exact-host stopped debug event. No pointer chain is
// traversed beyond the active usage record; the older-epoch record is not read.
TrackerSnapshot capture_depth_tracker(Read, void *, const Snapshot &work_item,
                                      Address preparation_context);
} // namespace mhw::exception_capture

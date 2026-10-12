#pragma once

namespace mhw::continuous_dlaa {
// The TAA request a native depth copy belongs to. A depth copy is leased (its
// destination copied into DLSS's private depth right after the native copy)
// only for the request that will evaluate it:
//  - the same projection stamp (frame, epoch, view, bank, async mode) and
//    container, and the request's depth texture as the copy's destination;
//  - in the scaled build, exactly the copy the request chose when it was
//    produced (depth_sequence: the newest live copy with that stamp and
//    destination). The DLSS evaluation then requires the lease on that copy's
//    recording.
// The title and loading screens record three copies a frame with the same
// stamp, source and destination, on different command lists (game-69/70). By
// stamp alone the first acceptable copy took the lease, the request's own copy
// was skipped (lease held), and the evaluation found the lease on another
// recording: DLSS skipped the frame (depth missing). In gameplay the extra
// copies differ in their resources, so the stamp alone already selected the
// request's copy (dlaa-identity-path.md).
template <class Request, class Copy>
constexpr bool depth_copy_belongs(const Request &t, const Copy &d) noexcept {
    const auto &s = t.source;
    if (!t.live || t.forwarding || s.frame != d.stamp.frame || s.epoch != d.stamp.epoch || s.view != d.stamp.view ||
        s.bank != d.stamp.bank || s.async != d.stamp.async || t.depth.container != d.stamp.container ||
        !(t.depth.texture == d.destination))
        return false;
    if constexpr (requires { t.depth_sequence; })
        return t.depth_sequence == d.sequence;
    else
        return true;
}
}  // namespace mhw::continuous_dlaa

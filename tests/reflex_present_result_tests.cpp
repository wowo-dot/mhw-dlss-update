#include "reflex/present_result.hpp"
#include "reflex/resize_contract.hpp"
#include "reflex/presentation_route.hpp"
#include "reflex/frame_generation_policy.hpp"
#include <dxgi.h>
#include <stdexcept>
#include <iostream>
using namespace mhw::reflex;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
int main() {
    try {
        require(generated_frames_supported(1, 1), "ordinary FG on ordinary-capable device");
        require(!generated_frames_supported(2, 1), "MFG rejected on ordinary-only device");
        require(generated_frames_supported(2, 5) && generated_frames_supported(3, 5) && generated_frames_supported(5, 5),
            "fixed MFG counts admitted within reported capability");
        require(!generated_frames_supported(6, 5), "unsupported multiplier rejected");
        require(!generated_frames_supported(0, 5) && !generated_frames_supported(1, 0), "missing mode/capability rejected");
        require(generated_frames_for(2, 5) == 1 && generated_frames_for(4, 5) == 3 && generated_frames_for(6, 5) == 5,
            "multiplier maps to generated frames within capability");
        require(generated_frames_for(8, 5) == 5 && generated_frames_for(4, 1) == 1, "multiplier limited to capability");
        require(generated_frames_for(4, 0) == 1, "unknown capability admits ordinary FG only");
        require(generated_frames_for(1, 5) == 0 && generated_frames_for(0, 5) == 0 && generated_frames_for(9, 5) == 0,
            "out-of-range multiplier unusable");
        require(parse_multiplier("2x") == 2 && parse_multiplier("6X") == 6 && parse_multiplier("8x") == 8, "multiplier text");
        require(!parse_multiplier("off") && !parse_multiplier("1x") && !parse_multiplier("9x") && !parse_multiplier("") &&
            !parse_multiplier("4") && !parse_multiplier("10x"), "non-multiplier text rejected");
        // game-124: a fault under SDK pacing keeps the game on the SDK's chain.
        require(fault_keeps_routing(PresentationContract::sdk_paced) && !fault_keeps_routing(PresentationContract::synchronous_native),
            "paced faults keep routing; synchronous faults fall back to the physical chain");
        require(presentation_result(PresentationContract::sdk_paced, 0, DXGI_ERROR_INVALID_CALL, S_OK).fault &&
            !presentation_result(PresentationContract::sdk_paced, 0, DXGI_ERROR_INVALID_CALL, S_OK).forward_native,
            "a failed paced Present is a fault, never a second native present");
        require(paced_host_call(0x1234, 0x1234, 7, 7), "owned logical chain routes on host thread");
        require(!paced_host_call(0x1234, 0x1234, 8, 7), "SDK worker on same physical chain stays native");
        require(!paced_host_call(0x5678, 0x1234, 7, 7), "another physical chain stays native");
        require(!paced_host_call(0x1234, 0x1234, 7, 0), "unestablished host owner stays native");
        require(!paced_host_call(0, 0, 7, 7), "retired host chain stays native");
        constexpr unsigned masks[]{1, 1};
        constexpr std::uintptr_t queues[]{0x1234, 0x1234}, changed[]{0x1234, 0x5678};
        constexpr unsigned other_node[]{1, 2};
        require(simple_resize_equivalent(2, 1, 0x1234, masks, queues), "unchanged single-node assignment is equivalent");
        require(!simple_resize_equivalent(2, 1, 0x1234, masks, changed), "queue reassignment must not be discarded");
        require(!simple_resize_equivalent(2, 1, 0x1234, other_node, queues), "different node mask rejected");
        require(!simple_resize_equivalent(2, 2, 0x1234, masks, queues), "multi-node resize requires separate support");
        require(!simple_resize_equivalent(0, 1, 0x1234, {}, {}), "unspecified buffer count not translated");
        require(!simple_resize_equivalent(17, 1, 0x1234, {}, {}), "bounded arguments");
        require(!simple_resize_equivalent(1, 1, 0x1234, masks, queues), "array count must match");
        require(!simple_resize_equivalent(2, 1, 0, masks, queues), "creation queue must be established");
        for (unsigned calls : {0U, 1U, 2U, 4U}) {
            const auto fg = presentation_result(PresentationContract::sdk_paced, calls, S_OK, E_FAIL);
            require(fg.result == S_OK && !fg.fault && !fg.forward_native, "pacer owns transaction; no duplicate host Present");
            const auto failed = presentation_result(PresentationContract::sdk_paced, calls, DXGI_ERROR_DEVICE_REMOVED, S_OK);
            require(failed.result == DXGI_ERROR_DEVICE_REMOVED && failed.fault && !failed.forward_native,
                "propagate SDK error without another native Present");
        }
        const auto occluded = presentation_result(PresentationContract::synchronous_native, 1, S_OK, DXGI_STATUS_OCCLUDED);
        require(occluded.result == DXGI_STATUS_OCCLUDED && !occluded.fault && !occluded.forward_native, "legacy native status preserved");
        const auto missing = presentation_result(PresentationContract::synchronous_native, 0, S_OK, E_FAIL);
        require(missing.fault && missing.forward_native, "legacy missing native call still forwards and faults");
        const auto duplicate = presentation_result(PresentationContract::synchronous_native, 2, S_OK, S_OK);
        require(duplicate.fault && !duplicate.forward_native, "legacy exact count still enforced");
        const auto sdk_error = presentation_result(PresentationContract::synchronous_native, 1, E_FAIL, S_OK);
        require(sdk_error.fault && !sdk_error.forward_native && sdk_error.result == S_OK, "legacy SDK error does not duplicate native call");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    return 0;
}

// SPDX-FileCopyrightText: Copyright (c) 2019-2026 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: MIT
// Public ABI subset adapted from NVIDIA/nvapi commit
// 70d337db9186e968eab622f7e786de7e437faf3d. See nvapi-sleep-status-LICENSE.txt.
// nvapi.h NV_GET_SLEEP_STATUS_PARAMS_V1; nvapi_lite_common.h NvBool/version;
// nvapi_interface.h NvAPI_D3D_GetSleepStatus. No NVAPI runtime is distributed.
#pragma once
#include <cstdint>
#include <cstddef>

namespace mhw::reflex::nvapi_abi {
#pragma pack(push, 8)
struct SleepStatus {
    std::uint32_t version;
    std::uint8_t low_latency, fullscreen_vrr, control_panel_vsync;
    std::uint32_t sleep_interval_us;
    std::uint8_t use_game_sleep, fullscreen_iflip, fg_multiplier, dfg_control;
    std::uint32_t dfg_frame_time_target_us;
    std::uint8_t reserved[114];
};
#pragma pack(pop)
static_assert(sizeof(SleepStatus) == 136 && alignof(SleepStatus) == 4);
static_assert(offsetof(SleepStatus, low_latency) == 4 && offsetof(SleepStatus, sleep_interval_us) == 8);
static_assert(offsetof(SleepStatus, reserved) == 20);
inline constexpr std::uint32_t sleep_status_version = sizeof(SleepStatus) | (1U << 16);
inline constexpr std::uint32_t sleep_status_id = 0xaef96ca1;
}

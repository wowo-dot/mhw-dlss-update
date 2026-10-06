#pragma once
#include "barrier_history.hpp"
namespace mhw::bridge_probe {
// One attempt, inside admitted DLSS Create with a verified command-list object.
// Hooks system D3D12 methods only; no game-code patch. No COM reference retained.
bool arm_barrier_observer(ID3D12GraphicsCommandList *, bool stop_after_completion = false) noexcept;
// Preflight only, before hooks: create the read-only external journal channel.
bool create_barrier_journal() noexcept;
// Pin a completed prefix at Evaluate, while continuing the same bounded trace.
// The label is recorded only; this function does not dereference the argument.
void checkpoint_barrier_observer(ID3D12GraphicsCommandList *, unsigned index = 0) noexcept;
// Stop admission explicitly (abort / quiescent fixture cleanup). Completed
// journal slots remain readable; already admitted callbacks may still finish.
void freeze_barrier_observer() noexcept;
BarrierCapture finish_barrier_observer(ID3D12GraphicsCommandList *,
                                       const EvaluateContractSnapshot &) noexcept;
#ifdef MHW_OWNED_OBSERVER_TEST
// Owned fixture only; absent from the deployed DLL.
void pause_owned_observer(BarrierCallKind, HANDLE entered, HANDLE resume) noexcept;
#endif
} // namespace mhw::bridge_probe

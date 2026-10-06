#pragma once
#include <windows.h>

namespace mhw::reflex {
enum class PresentationContract { synchronous_native, sdk_paced };
struct PresentDecision { HRESULT result; bool fault; bool forward_native; };
// The FG pacer may present another swap chain, another thread, or multiple
// frames. Absence of a TLS-observed native call is never permission to present
// the host frame again. The SDK owns the paced transaction and its HRESULT.
constexpr PresentDecision presentation_result(PresentationContract contract, unsigned observed_native_calls,
                                               HRESULT sdk_result, HRESULT native_result) noexcept {
    if (contract == PresentationContract::sdk_paced) return {sdk_result, FAILED(sdk_result), false};
    return {native_result, observed_native_calls != 1 || FAILED(sdk_result), observed_native_calls == 0};
}
}

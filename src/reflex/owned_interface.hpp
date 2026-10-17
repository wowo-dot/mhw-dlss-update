#pragma once
#include <sl_result.h>
#include <wrl/client.h>

namespace mhw::reflex {
// Streamline 2.12's proxy constructors AddRef the native interface. The input
// reference is NOT transferred by slUpgradeInterface. Adopt the proxy's new
// reference and release our old native reference, including failure rollback.
// The caller supplies an owned COM reference, never a borrowed global slot.
template <class T, class Upgrade> sl::Result upgrade_owned(Microsoft::WRL::ComPtr<T> &owned, Upgrade &&upgrade) {
    auto *native = owned.Get();
    if (!native) return sl::Result::eErrorMissingInputParameter;
    void *replacement = native;
    const auto result = upgrade(&replacement);
    if (result != sl::Result::eOk || !replacement) {
        if (replacement && replacement != native) static_cast<T *>(replacement)->Release();
        return result == sl::Result::eOk ? sl::Result::eErrorInvalidState : result;
    }
    if (replacement != native) owned.Attach(static_cast<T *>(replacement));
    return sl::Result::eOk;
}
} // namespace mhw::reflex

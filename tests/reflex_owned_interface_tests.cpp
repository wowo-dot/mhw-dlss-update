#include "reflex/owned_interface.hpp"
#include <stdexcept>
#include <iostream>

namespace {
void require(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
struct Counts {
    unsigned objects{}, refs{};
};
// Models the documented proxy AddRef plus a separately owned input reference.
class Object final : public IUnknown {
public:
    explicit Object(Counts &counts, IUnknown *native = nullptr) : counts_(counts), native_(native) {
        ++counts_.objects;
        ++counts_.refs;
        if (native_) native_->AddRef();
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **value) override {
        if (value) *value = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        ++counts_.refs;
        return ++refs_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        --counts_.refs;
        const auto refs = --refs_;
        if (!refs) delete this;
        return refs;
    }

private:
    ~Object() {
        if (native_) native_->Release();
        --counts_.objects;
    }
    Counts &counts_;
    IUnknown *native_{};
    ULONG refs_{1};
};
void tests() {
    for (unsigned mode = 0; mode < 4; ++mode) {
        Counts counts;
        Microsoft::WRL::ComPtr<IUnknown> owned;
        owned.Attach(new Object(counts));
        const auto *initial = owned.Get();
        const auto result = mhw::reflex::upgrade_owned(owned, [&](void **value) {
            if (mode == 0 || mode == 2) *value = new Object(counts, static_cast<IUnknown *>(*value));
            if (mode == 3) *value = nullptr;
            return mode == 2 ? sl::Result::eErrorInvalidIntegration : sl::Result::eOk;
        });
        if (mode == 0)
            require(result == sl::Result::eOk && owned.Get() != initial && counts.refs == 2,
                    "proxy replaces exactly one owned reference");
        if (mode == 1)
            require(result == sl::Result::eOk && owned.Get() == initial && counts.refs == 1,
                    "unchanged native reference preserved");
        if (mode >= 2)
            require(result != sl::Result::eOk && owned.Get() == initial && counts.refs == 1,
                    "failure retains original and releases partial proxy");
        owned.Reset();
        require(counts.refs == 0 && counts.objects == 0, "complete ownership teardown");
    }
    Microsoft::WRL::ComPtr<IUnknown> empty;
    bool called{};
    require(mhw::reflex::upgrade_owned(empty,
                                       [&](void **) {
                                           called = true;
                                           return sl::Result::eOk;
                                       }) == sl::Result::eErrorMissingInputParameter &&
                !called,
            "missing owned input rejected");
}
} // namespace
int main() {
    try {
        tests();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}

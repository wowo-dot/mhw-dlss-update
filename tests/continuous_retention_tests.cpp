#include "../src/continuous_dlaa/retention.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <sstream>
using namespace mhw::continuous_dlaa;
void check(bool v, const char *why) {
    if (!v) {
        std::cerr << why << '\n';
        std::exit(1);
    }
}
// This fixture tests only COM ownership: RecordingPins never calls resource APIs.
struct Owned : IUnknown {
    ULONG count{1};
    unsigned &destroyed;
    explicit Owned(unsigned &n) : destroyed(n) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++count; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto n = --count;
        if (!n) {
            ++destroyed;
            delete this;
        }
        return n;
    }
};
ResourcePins make(unsigned &destroyed) {
    ResourcePins p;
    for (auto &r : p) r.Attach(reinterpret_cast<ID3D12Resource *>(new Owned(destroyed)));
    return p;
}
int main() {
    auto *list = reinterpret_cast<ID3D12GraphicsCommandList *>(0x1000);
    unsigned destroyed{};
    RecordingPins records;
    auto resources = make(destroyed);
    check(!records.retain(nullptr, 1, resources) && !records.retain(list, 0, resources), "invalid recording");
    check(records.retain(list, 1, resources) && !records.retain(list, 1, resources), "duplicate generation rejected");
    check(records.matches(list, 1, resources), "exact resources retained");
    check(!records.reset(list, 1), "unsubmitted Reset has no retirement proof");
    records.collect(999);
    check(!destroyed && records.live() == 1, "unsubmitted references retained");
    check(!records.submitted(list, 1, 0) && !records.submitted(list, 1, UINT64_MAX), "invalid signals rejected");
    check(records.submitted(list, 1, 10) && !records.submitted(list, 1, 11), "submission exactly once");
    resources = {};
    records.collect(10);
    check(!destroyed, "completion alone cannot release replayable commands");
    check(records.reset(list, 1), "successful Reset proof");
    records.collect(9);
    check(!destroyed, "Reset cannot release pending GPU work");
    records.collect(UINT64_MAX);
    check(!destroyed, "removed device is not completion");
    records.collect(10);
    check(destroyed == 4 && records.live() == 0 && records.released() == 1, "both proofs release every reference");
    // Same list with old work still pending: generations must remain independent.
    for (unsigned i = 2; i <= 129; ++i) {
        auto p = make(destroyed);
        check(records.retain(list, i, p), "bounded live generation");
        check(records.submitted(list, i, i + 10) && records.reset(list, i), "submitted then reset, GPU pending");
    }
    auto extra = make(destroyed);
    check(!records.retain(list, 130, extra), "capacity never evicts live work");
    extra = {};
    records.collect(74);
    check(records.live() == 65, "only covered fences released");
    records.collect(139);
    check(records.live() == 0 && records.released() == 129, "all completed old generations released");
    // Repeated transitions far beyond both old destination and runtime-pin caps.
    for (unsigned i = 130; i < 2130; ++i) {
        auto p = make(destroyed);
        check(records.retain(list, i, p), "reusable bounded capacity");
        p = {};
        check(records.submitted(list, i, i) && records.reset(list, i), "stream receipts");
        records.collect(i);
    }
    check(records.live() == 0 && records.released() == 2129 && destroyed == 4 * (2129 + 1),
          "2000 transitions leak no references");
    std::ostringstream out;
    records.write(out, 2129);
    std::cout << out.str() << '\n';
}

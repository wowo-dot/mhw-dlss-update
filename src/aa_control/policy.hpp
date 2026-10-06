#pragma once
#include <optional>
#include <string_view>

namespace mhw::aa_control {
// Renderer integration must apply decisions at its frame boundary, before
// projection/jitter. No game option is an input. DLSS is deliberately rejected
// until the resolution-changing implementation exists.
enum class Mode {native,off,dlaa};
enum class Path {native,unfiltered,reconstruct};
enum class State {native,off,waiting,active,failed};
struct Decision {Path path;bool jitter{},reset_history{};};
inline std::optional<Mode> parse_mode(std::string_view text) noexcept {
    if(text=="native")return Mode::native;
    if(text=="off")return Mode::off;
    if(text=="dlaa")return Mode::dlaa;
    return {};
}
// Serialized render-controller use only. Calling select() is an explicit mode
// change/retry; transient availability never clears a latched failure.
class Policy {
public:
    void select(Mode mode) noexcept {
        mode_=mode;failed_=false;reset_=true;pending_=false;
        state_=mode==Mode::native?State::native:mode==Mode::off?State::off:State::waiting;
    }
    Decision begin(bool inputs_ready) noexcept {
        if(pending_){failed_=true;state_=State::failed;pending_=false;}
        if(mode_==Mode::native)return {Path::native};
        if(mode_==Mode::off)return {Path::unfiltered};
        if(failed_)return {Path::unfiltered};
        if(!inputs_ready){reset_=true;state_=State::waiting;return {Path::unfiltered};}
        pending_=true;return {Path::reconstruct,true,reset_};
    }
    void complete(bool success) noexcept {
        if(!pending_||!success){failed_=true;reset_=true;state_=State::failed;}
        else {reset_=false;state_=State::active;}
        pending_=false;
    }
    State state() const noexcept {return state_;}
private:
    Mode mode_{Mode::native};State state_{State::native};
    bool failed_{},reset_{true},pending_{};
};
// Only for a qualified invocation whose unfiltered route is already valid.
// The caller handles unrelated dispatches outside this helper. No implicit TAA
// fallback exists; unfiltered failure must escalate to the renderer controller.
template<class Reconstruct,class Unfiltered,class Native>
bool route(Decision plan,Reconstruct reconstruct,Unfiltered unfiltered,Native native){
    if(plan.path==Path::native){native();return true;}
    if(plan.path==Path::reconstruct&&reconstruct())return true;
    return unfiltered();
}
}

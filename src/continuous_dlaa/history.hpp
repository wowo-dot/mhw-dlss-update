#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <ostream>
namespace mhw::continuous_dlaa {
struct HistoryStamp {
    std::uint64_t frame{},epoch{},microseconds{},camera{},view{},depth{};
    std::array<unsigned,2> viewport{};
    std::array<float,16> view_matrix{};
    std::array<unsigned,16> projection{};
};
enum class ResetReason : unsigned {first,frame_gap,epoch_gap,camera,view,depth,viewport,projection,interval,view_other,camera_displacement,count};
constexpr unsigned reset_reason_count=static_cast<unsigned>(ResetReason::count);
constexpr std::array<const char *,reset_reason_count> reset_reason_names{
    "first","frame_gap","epoch_gap","camera","view","depth","viewport","projection","interval","view_other","camera_displacement"};
constexpr unsigned reason_bit(ResetReason r){return 1U<<static_cast<unsigned>(r);}
enum class HistoryPolicy {camera_displacement,distance_only_comparison};
struct HistoryDecision {
    bool valid{},reset{};float milliseconds{};
    unsigned reasons{},projection_mask{},view_jump_mask{};
    std::array<double,3> previous_camera{},next_camera{};
    double camera_distance{};
    unsigned control_reasons{};bool displacement_only_suppressed{};
};
// Row-vector affine view: C*R+t=0. Solve the actual float matrix in double
// precision instead of assuming that its transpose is an exact inverse.
inline bool camera_position(const std::array<float,16> &m,std::array<double,3> &center) noexcept {
    for(float v:m)if(!std::isfinite(v))return false;
    if(m[3]!=0||m[7]!=0||m[11]!=0||m[15]!=1)return false;
    using V=std::array<double,3>;
    const V a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[8],m[9],m[10]},rhs{-double(m[12]),-double(m[13]),-double(m[14])};
    const auto dot=[](const V &x,const V &y){return x[0]*y[0]+x[1]*y[1]+x[2]*y[2];};
    const auto cross=[](const V &x,const V &y){return V{x[1]*y[2]-x[2]*y[1],x[2]*y[0]-x[0]*y[2],x[0]*y[1]-x[1]*y[0]};};
    const std::array<V,3> rows{a,b,c};
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
        if(std::abs(dot(rows[i],rows[j])-(i==j?1.:0.))>1.e-4)return false;
    const auto bc=cross(b,c),ca=cross(c,a),ab=cross(a,b);const auto determinant=dot(a,bc);
    if(std::abs(determinant-1.)>1.e-4)return false;
    const V solved{dot(rhs,bc)/determinant,dot(rhs,ca)/determinant,dot(rhs,ab)/determinant};
    for(unsigned j=0;j<3;++j){
        if(!std::isfinite(solved[j]))return false;
        const double residual=solved[0]*a[j]+solved[1]*b[j]+solved[2]*c[j]-rhs[j];
        if(std::abs(residual)>1.e-9*(1.+std::abs(rhs[j])))return false;
    }
    center=solved;return true;
}
// A projection change is a cut when an element appears, disappears, changes
// sign or is not finite, or moves by more than 10% of its magnitude in one
// frame. Smaller changes are FOV and clip-plane animation (zoom easing, the
// windrake ride: a reset on every such frame stopped FG in game-38); motion
// vectors include them, so DLSS and FG keep their history.
inline bool projection_cut(const std::array<unsigned,16> &before,const std::array<unsigned,16> &after) noexcept {
    for(unsigned i=0;i<16;++i){if(before[i]==after[i])continue;
        const auto a=std::bit_cast<float>(before[i]),b=std::bit_cast<float>(after[i]);
        if(!std::isfinite(a)||!std::isfinite(b)||(a==0)!=(b==0)||std::signbit(a)!=std::signbit(b))return true;
        if(std::abs(b-a)>.1F*std::fmax(std::abs(a),std::abs(b)))return true;}
    return false;
}
// Identity/continuity/projection/interval and nontranslation rotation checks
// remain unchanged. Translation cuts use Euclidean camera displacement >20.
// The opt-in comparison removes only a displacement-only reset. It is a
// diagnostic ablation, not a validated camera-cut policy: pure translation
// cuts can also match. Malformed matrices still invalidate admission.
inline HistoryDecision history_decision(const HistoryStamp &previous,const HistoryStamp &next,
    HistoryPolicy policy=HistoryPolicy::camera_displacement) noexcept {
    if(!next.frame||!next.epoch||!next.microseconds||!next.camera||!next.view||!next.depth)return {};
    std::array<double,3> before{},after{};
    if(!camera_position(next.view_matrix,after)||(previous.frame&&!camera_position(previous.view_matrix,before)))return {};
    unsigned reasons{},projection_mask{},view_jump_mask{};
    const auto mark=[&](bool yes,ResetReason r){if(yes)reasons|=reason_bit(r);};
    mark(!previous.frame,ResetReason::first);
    mark(next.frame!=previous.frame+1,ResetReason::frame_gap);
    mark(next.epoch!=previous.epoch+1,ResetReason::epoch_gap);
    mark(next.camera!=previous.camera,ResetReason::camera);
    mark(next.view!=previous.view,ResetReason::view);
    mark(next.depth!=previous.depth,ResetReason::depth);
    mark(next.viewport!=previous.viewport,ResetReason::viewport);
    for(unsigned i=0;i<16;++i)if(next.projection[i]!=previous.projection[i])projection_mask|=1U<<i;
    mark(projection_cut(previous.projection,next.projection),ResetReason::projection);
    if(previous.frame&&next.frame<=previous.frame)return {};
    float ms=16.666667F;
    if(previous.microseconds){
        if(next.microseconds<=previous.microseconds)return {};
        ms=static_cast<float>(next.microseconds-previous.microseconds)/1000.F;
        mark(ms>250.F,ResetReason::interval);
        ms=(ms<.1F)? .1F : (ms>1000.F?1000.F:ms);
        for(unsigned i=0;i<16;++i){const auto difference=std::abs(next.view_matrix[i]-previous.view_matrix[i]);
            const bool translation=i>=12&&i<=14;
            if(difference>(translation?20.F:.5F)){
                view_jump_mask|=1U<<i;if(!translation)mark(true,ResetReason::view_other);}}
    }
    double distance{};
    if(previous.frame){for(unsigned i=0;i<3;++i){const auto delta=after[i]-before[i];distance+=delta*delta;}
        distance=std::sqrt(distance);if(!std::isfinite(distance))return {};
        mark(distance>20.,ResetReason::camera_displacement);}
    // The first evaluation always resets; identity differences from an empty
    // predecessor are not useful reset diagnoses.
    if(!previous.frame)reasons=reason_bit(ResetReason::first);
    const auto control_reasons=reasons;
    const bool suppressed=policy==HistoryPolicy::distance_only_comparison&&reasons==reason_bit(ResetReason::camera_displacement);
    if(suppressed)reasons=0;
    return {true,reasons!=0,ms,reasons,projection_mask,view_jump_mask,before,after,distance,control_reasons,suppressed};
}

struct HistoryExample {HistoryStamp previous,next;HistoryDecision decision;};
// Updated under the runtime's existing lock after a successful evaluation.
// One first example per reason plus the latest reset; no growing frame journal.
struct HistoryTelemetry {
    HistoryPolicy policy{HistoryPolicy::camera_displacement};
    std::uint64_t evaluations{},resets{},retained{},retained_streak{},longest_retained_streak{};
    std::array<std::uint64_t,reset_reason_count> counts{};
    std::array<HistoryExample,reset_reason_count> first{};
    HistoryExample latest;
    std::uint64_t translation_resets_avoided{};
    HistoryExample first_avoided,latest_avoided;
    std::uint64_t projection_changes_retained{};  // projection changed below the cut threshold
    std::uint64_t displacement_only_resets_suppressed{};
    HistoryExample first_displacement_suppressed,latest_displacement_suppressed;
    void record(const HistoryStamp &before,const HistoryStamp &after,const HistoryDecision &d) noexcept {
        if(!d.valid)return;
        ++evaluations;
        if(d.displacement_only_suppressed){latest_displacement_suppressed={before,after,d};
            if(!displacement_only_resets_suppressed)first_displacement_suppressed=latest_displacement_suppressed;
            ++displacement_only_resets_suppressed;}
        if(!d.reset){++retained;++retained_streak;if(retained_streak>longest_retained_streak)longest_retained_streak=retained_streak;
            if(d.projection_mask)++projection_changes_retained;
            if(d.view_jump_mask&0x7000U){latest_avoided={before,after,d};if(!translation_resets_avoided)first_avoided=latest_avoided;++translation_resets_avoided;}return;}
        ++resets;retained_streak=0;latest={before,after,d};
        for(unsigned i=0;i<reset_reason_count;++i)if(d.reasons&(1U<<i)){
            if(!counts[i])first[i]=latest;++counts[i];}
    }
    static void stamp(std::ostream &out,const HistoryStamp &s){
        out<<"{\"frame\":"<<s.frame<<",\"epoch\":"<<s.epoch<<",\"microseconds\":"<<s.microseconds
           <<",\"camera\":"<<s.camera<<",\"view\":"<<s.view<<",\"depth\":"<<s.depth
           <<",\"viewport\":["<<s.viewport[0]<<','<<s.viewport[1]<<"],\"view_bits\":[";
        for(unsigned i=0;i<16;++i){if(i)out<<',';out<<std::bit_cast<unsigned>(s.view_matrix[i]);}
        out<<"],\"projection_bits\":[";for(unsigned i=0;i<16;++i){if(i)out<<',';out<<s.projection[i];}out<<"]}";
    }
    static void example(std::ostream &out,const HistoryExample &e){
        out<<"{\"reasons\":"<<e.decision.reasons<<",\"control_reasons\":"<<e.decision.control_reasons
           <<",\"displacement_only_suppressed\":"<<(e.decision.displacement_only_suppressed?"true":"false")<<",\"projection_mask\":"<<e.decision.projection_mask
           <<",\"view_jump_mask\":"<<e.decision.view_jump_mask<<",\"camera_distance\":"<<e.decision.camera_distance<<",\"previous_camera\":[";
        for(unsigned i=0;i<3;++i){if(i)out<<',';out<<e.decision.previous_camera[i];}
        out<<"],\"next_camera\":[";for(unsigned i=0;i<3;++i){if(i)out<<',';out<<e.decision.next_camera[i];}out<<"],\"previous\":";
        stamp(out,e.previous);out<<",\"next\":";stamp(out,e.next);out<<'}';
    }
    void write(std::ostream &out) const {
        const bool comparison=policy==HistoryPolicy::distance_only_comparison;
        out<<std::setprecision(17)<<"{\"schema\":"<<(comparison?3:2)<<",\"policy\":\""<<(comparison?"distance-only-comparison-v1":"camera-displacement-v1")
           <<"\",\"evaluations\":"<<evaluations<<",\"resets\":"<<resets
           <<",\"retained\":"<<retained<<",\"retained_streak\":"<<retained_streak<<",\"longest_retained_streak\":"<<longest_retained_streak<<",\"counts\":{";
        for(unsigned i=0;i<reset_reason_count;++i){if(i)out<<',';out<<'"'<<reset_reason_names[i]<<"\":"<<counts[i];}
        out<<"},\"first\":[";bool comma=false;
        for(unsigned i=0;i<reset_reason_count;++i)if(counts[i]){
            if(comma)out<<',';comma=true;out<<"{\"reason_index\":"<<i<<",\"example\":";example(out,first[i]);out<<'}';}
        out<<"],\"latest\":";if(resets)example(out,latest);else out<<"null";
        out<<",\"translation_resets_avoided\":"<<translation_resets_avoided<<",\"first_avoided\":";
        if(translation_resets_avoided)example(out,first_avoided);else out<<"null";
        out<<",\"latest_avoided\":";if(translation_resets_avoided)example(out,latest_avoided);else out<<"null";
        out<<",\"projection_changes_retained\":"<<projection_changes_retained;
        if(comparison){
            out<<",\"diagnostic_only\":true,\"control_resets\":"<<resets+displacement_only_resets_suppressed
               <<",\"displacement_only_resets_suppressed\":"<<displacement_only_resets_suppressed<<",\"first_displacement_suppressed\":";
            if(displacement_only_resets_suppressed)example(out,first_displacement_suppressed);else out<<"null";
            out<<",\"latest_displacement_suppressed\":";if(displacement_only_resets_suppressed)example(out,latest_displacement_suppressed);else out<<"null";
        }
        out<<'}';
    }
};
}

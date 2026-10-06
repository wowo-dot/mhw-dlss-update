#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <iosfwd>
namespace mhw::projection_binding {
inline constexpr unsigned max_barriers=64,max_resources=9;
struct ViewBinding {
  std::uintptr_t owner{},backend{},resource{},handle{};
  unsigned format{},rotating{},rotation_index{};
  bool operator==(const ViewBinding &) const = default;
};
struct NativePipeline {
  std::uintptr_t render{},graphics{},shader_handle{},input_layout{},input_elements{};
  std::uintptr_t depth_owner{},depth_state{},blend_owner{},blend_state{},raster_owner{},raster_state{};
  unsigned input_count{},graphics_flags{},group{},raster_variant{},dsv_format{};
  std::array<unsigned,13> depth{};
  std::array<unsigned,82> blend{};
  std::array<unsigned,11> raster{};
  bool operator==(const NativePipeline &) const = default;
};
struct AttachmentSnapshot {
  bool valid{};
  std::uintptr_t state{},command{},attachment{},active_attachment{},incoming_pso{};
  unsigned color_count{},attachment_flags{},scissor_enabled{};
  std::array<ViewBinding,9> views{}; // eight color slots, then depth
  std::array<std::uintptr_t,9> cached{};
  std::uintptr_t cached_depth_resource{};
  std::array<int,4> viewport{},scissor{};
  NativePipeline pipeline{};
  bool operator==(const AttachmentSnapshot &) const = default;
};
struct ResourceDescription {
  bool valid{};
  std::uintptr_t resource{},identity{},device{};
  unsigned dimension{},format{},height{},depth_or_array{},mips{},samples{},quality{},layout{},flags{};
  std::uint64_t width{},alignment{};
  bool operator==(const ResourceDescription &) const = default;
};
struct BarrierRecord {
  unsigned type{},flags{},subresource{},before{},after{};
  std::uintptr_t resource{},other{};
};
// Checkpoint is the last attempted operation, 15 means fully validated.
// HRESULT slots: resource QI, GetDevice, canonical QI, canonical self-QI,
// resource roundtrip QI, roundtrip canonical QI. Unattempted = E_PENDING.
struct ResourceQuery {
  unsigned checkpoint{};
  bool valid{},temporaries_released{};
  std::uintptr_t candidate{},resource{},device{},identity{},identity_repeat{},roundtrip{},roundtrip_identity{};
  std::array<unsigned,6> results{0x8000000a,0x8000000a,0x8000000a,0x8000000a,0x8000000a,0x8000000a};
};
struct AttachmentCapture {
  bool enabled{},described{},at_draw{},stable{},resources_released{};
  std::uint64_t state_order{},draw_order{};
  AttachmentSnapshot entry{},ready{};
  std::array<ResourceDescription,max_resources> resources{};
  unsigned resource_count{},barrier_count{};
  // Initial qualification, draw-API entry, native-draw return; includes the
  // failing attempt even when no ResourceDescription is accepted.
  std::array<std::array<ResourceQuery,max_resources>,3> resource_checks{};
  std::array<unsigned,3> resource_check_counts{};
};
struct OutputBinding {
  unsigned count{},contiguous{};
  bool depth_present{};
  std::array<std::uintptr_t,8> colors{};
  std::uintptr_t depth{};
};
struct PipelineCreation {
  bool seen{},iid_valid{},bytecode_matches{},returned{},succeeded{},stable{};
  std::uintptr_t descriptor_address{},output_address{},pso{},root{};
  unsigned result{},input_count{},color_count{},dsv_format{},samples{},quality{};
  std::array<unsigned,8> formats{};
  std::array<unsigned,5> sizes{};
  std::array<std::uintptr_t,5> data{};
  std::array<std::byte,656> descriptor{};
  std::array<std::byte,1280> input_elements{};
  std::array<std::array<char,64>,32> semantics{};
};
struct Draw;struct Frame;struct Capture;
// Immutable CPU-side draw recipe, without consulting a recording's caches.
bool snapshot_native_pipeline(std::uintptr_t image,std::uintptr_t packet,NativePipeline &,unsigned depth_format=0) noexcept;
bool snapshot_attachments(std::uintptr_t,const Draw &,AttachmentSnapshot &) noexcept;
bool attachment_inputs_equal(const AttachmentSnapshot &,const AttachmentSnapshot &) noexcept;
bool attachment_cache_matches(const AttachmentSnapshot &) noexcept;
bool describe_attachments(AttachmentCapture &,std::uintptr_t) noexcept;
bool recheck_resources(AttachmentCapture &,unsigned) noexcept;
bool release_attachment_resources() noexcept;
bool capture_output_binding(const Frame &,OutputBinding &) noexcept;
bool output_binding_matches(const OutputBinding &,const AttachmentSnapshot &) noexcept;
bool capture_barriers(const Frame &,std::array<BarrierRecord,max_barriers> &,unsigned &) noexcept;
bool capture_pipeline_creation(const Frame &,const Draw &,const Capture &,PipelineCreation &) noexcept;
bool finish_pipeline_creation(const Frame &,const Draw &,const Capture &,PipelineCreation &) noexcept;
void write_attachments(std::ostream &,const AttachmentCapture &);
void write_output_binding(std::ostream &,const OutputBinding &);
void write_barriers(std::ostream &,const std::array<BarrierRecord,max_barriers> &,unsigned);
void write_pipeline_creation(std::ostream &,const PipelineCreation &);
bool system_method(std::uintptr_t,unsigned,std::uintptr_t &) noexcept;
}

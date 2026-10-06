#pragma once
#include "inputs.hpp"
namespace mhw::temporal_bindings {
inline constexpr unsigned shader_limit=262144, owner_limit=64, binding_limit=128;
struct ShaderOwner {
  unsigned id{},mode{};std::uintptr_t object{};
  bool operator==(const ShaderOwner &)const=default;
};
struct ShaderBinding {
  unsigned word{};std::uintptr_t view{},cpu_descriptor{};
  bool operator==(const ShaderBinding &)const=default;
};
struct ShaderRecipe {
  std::uintptr_t state{},handle{},set{},entry{},descriptor{},bytecode{},ids{},modes{},words{};
  unsigned size{},technique{},owner_count{},binding_count{},binding_start{};
  std::array<ShaderOwner,owner_limit> owners{};
  std::array<ShaderBinding,binding_limit> bindings{};
  bool operator==(const ShaderRecipe &)const=default;
};
// CPU request metadata only. A descriptor address is not a typed view description.
bool shader_recipe(const Reader &,const Snapshot &,ShaderRecipe &);
bool compute_dxbc(const std::byte *,unsigned) noexcept;
}

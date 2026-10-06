#include "overlay_extent.hpp"
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mhw::scaled_temporal {
namespace {
unsigned float_bytes(DXGI_FORMAT f) noexcept {
    switch(f){case DXGI_FORMAT_R32G32_FLOAT:return 8;case DXGI_FORMAT_R32G32B32_FLOAT:return 12;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:return 16;default:return 0;}
}
// Used only to resolve APPEND_ALIGNED_ELEMENT. Unknown native attributes can
// remain untouched when offsets are explicit, but cannot define an append.
unsigned element_bytes(DXGI_FORMAT f) noexcept {
    switch(f){
    case DXGI_FORMAT_R32G32B32A32_FLOAT:case DXGI_FORMAT_R32G32B32A32_UINT:case DXGI_FORMAT_R32G32B32A32_SINT:return 16;
    case DXGI_FORMAT_R32G32B32_FLOAT:case DXGI_FORMAT_R32G32B32_UINT:case DXGI_FORMAT_R32G32B32_SINT:return 12;
    case DXGI_FORMAT_R32G32_FLOAT:case DXGI_FORMAT_R32G32_UINT:case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R16G16B16A16_UNORM:case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:case DXGI_FORMAT_R16G16B16A16_SINT:return 8;
    case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R32_UINT:case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_SNORM:case DXGI_FORMAT_R8G8B8A8_UINT:case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_FLOAT:case DXGI_FORMAT_R16G16_UNORM:case DXGI_FORMAT_R16G16_SNORM:case DXGI_FORMAT_R16G16_UINT:case DXGI_FORMAT_R16G16_SINT:return 4;
    default:return 0;
    }
}
}
bool overlay_vertex_layout(std::span<const D3D12_INPUT_ELEMENT_DESC> elements,unsigned stride,OverlayVertexLayout &out) noexcept {
    out={};if(stride<16||stride>255||elements.size()<2||elements.size()>32)return false;
    OverlayVertexLayout result{stride};bool position=false,uv=false;unsigned previous_end{};
    for(const auto &e:elements){
        if(!e.SemanticName||e.InputSlot||e.InputSlotClass!=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA||e.InstanceDataStepRate)return false;
        const auto bytes=element_bytes(e.Format);auto offset=e.AlignedByteOffset;
        // Accept appends only when the preceding end is already four-byte
        // aligned; this avoids guessing alignment for packed/unknown formats.
        if(offset==D3D12_APPEND_ALIGNED_ELEMENT){if(previous_end==UINT_MAX||previous_end%4)return false;offset=previous_end;}
        if(offset>stride||(bytes&&bytes>stride-offset))return false;
        previous_end=bytes?offset+bytes:UINT_MAX;
        const bool is_position=_stricmp(e.SemanticName,"POSITION")==0&&e.SemanticIndex==0;
        const bool is_uv=_stricmp(e.SemanticName,"TEXCOORD")==0&&e.SemanticIndex==0;
        if(!is_position&&!is_uv)continue; // unused by the pinned VS
        if(!float_bytes(e.Format)||offset%4)return false;
        if(is_position){if(position)return false;position=true;result.position=offset;result.position_format=e.Format;}
        else{if(uv)return false;uv=true;result.uv=offset;result.uv_format=e.Format;}
    }
    if(!position||!uv||(result.position<result.uv+8&&result.uv<result.position+8))return false;
    out=result;return true;
}
bool overlay_vertices(std::span<const std::byte> bytes,const OverlayVertexLayout &layout,std::span<float,16> out) noexcept {
    const auto p=float_bytes(layout.position_format),u=float_bytes(layout.uv_format);
    if(layout.stride<16||layout.stride>255||!p||!u||layout.position>layout.stride-p||layout.uv>layout.stride-u||
       bytes.size()!=4*layout.stride)return false;
    std::array<float,16> decoded{};
    for(unsigned i=0;i<4;++i){std::memcpy(decoded.data()+4*i,bytes.data()+i*layout.stride+layout.position,8);
        std::memcpy(decoded.data()+4*i+2,bytes.data()+i*layout.stride+layout.uv,8);}
    if(!overlay_coverage(decoded))return false;
    std::copy(decoded.begin(),decoded.end(),out.begin());return true;
}
bool overlay_pixel_shader(std::span<const std::byte> bytes){
    if(bytes.size()<52||bytes.size()>262144)return false;
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
    if(FAILED(D3DReflect(bytes.data(),bytes.size(),IID_PPV_ARGS(&reflection))))return false;
    D3D12_SHADER_DESC shader{};if(FAILED(reflection->GetDesc(&shader))||shader.InputParameters>16||shader.OutputParameters!=1||shader.BoundResources>64)return false;
    D3D12_SIGNATURE_PARAMETER_DESC out{};
    if(FAILED(reflection->GetOutputParameterDesc(0,&out))||out.SystemValueType!=D3D_NAME_TARGET||out.SemanticIndex||out.Register)return false;
    for(unsigned i=0;i<shader.InputParameters;++i){D3D12_SIGNATURE_PARAMETER_DESC in{};
        if(FAILED(reflection->GetInputParameterDesc(i,&in)))return false;
        if(in.ReadWriteMask&&(!in.SemanticName||_stricmp(in.SemanticName,"TEXCOORD")||in.SemanticIndex||in.SystemValueType!=D3D_NAME_UNDEFINED||in.ComponentType!=D3D_REGISTER_COMPONENT_FLOAT32||(in.ReadWriteMask&~3U)))return false;}
    for(unsigned i=0;i<shader.BoundResources;++i){D3D12_SHADER_INPUT_BIND_DESC b{};
        if(FAILED(reflection->GetResourceBindingDesc(i,&b))||(b.Type!=D3D_SIT_CBUFFER&&b.Type!=D3D_SIT_SAMPLER&&b.Type!=D3D_SIT_TEXTURE))return false;}
    return true;
}
bool overlay_coverage(std::span<const float> v) noexcept {
    // Exact native strip coverage. Arbitrary quads, offsets or partial effects
    // are not promoted to a full-screen overlay. UV values remain untouched.
    if(v.size()!=16)return false;
    for(float f:v)if(!std::isfinite(f))return false;
    constexpr float column[]{-1,1,-1,-1,1,1,1,-1};
    constexpr float row[]{-1,1,1,1,-1,-1,1,-1};
    bool a=true,b=true;for(unsigned i=0;i<4;++i)for(unsigned j=0;j<2;++j){
        a=a&&v[4*i+j]==column[2*i+j];b=b&&v[4*i+j]==row[2*i+j];}
    return a||b;
}
bool overlay_raster(const EffectBindings &b,FilterGeometry g) noexcept {
    return valid_filter_geometry(g)&&b.count==4&&b.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP&&
        b.viewport.TopLeftX==0&&b.viewport.TopLeftY==0&&b.viewport.Width==g.render_width&&b.viewport.Height==g.render_height&&
        b.viewport.MinDepth==0&&b.viewport.MaxDepth==1&&!b.scissor.left&&!b.scissor.top&&
        b.scissor.right==static_cast<LONG>(g.render_width)&&b.scissor.bottom==static_cast<LONG>(g.render_height);
}
void record_overlay(const EffectBindings &b,FilterGeometry g) noexcept {
    const D3D12_VIEWPORT viewport{0,0,float(g.output_width),float(g.output_height),0,1};
    const D3D12_RECT scissor{0,0,static_cast<LONG>(g.output_width),static_cast<LONG>(g.output_height)};
    b.list->RSSetViewports(1,&viewport);b.list->RSSetScissorRects(1,&scissor);
    b.list->DrawInstanced(b.count,1,b.first,0);
    b.list->RSSetViewports(1,&b.viewport);b.list->RSSetScissorRects(1,&b.scissor);
}
}

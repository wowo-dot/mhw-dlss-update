#pragma once
namespace mhw::temporal_gpu_inputs {
inline constexpr char shader_source[] = R"(
cbuffer Dimensions : register(b0) { uint width; uint height; };
Texture2D<float3> color_in : register(t0);
Texture2D<float> depth_in : register(t1);
Texture2D<uint> motion_in : register(t2);
RWTexture2D<float4> color_out : register(u0);
RWTexture2D<float> depth_out : register(u1);
RWTexture2D<float2> motion_out : register(u2);
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
    uint p = motion_in.Load(int3(id.xy, 0));
    // Verified MHW history offset: previous_pixel = current_pixel + offset.
    // The low bit of the lower half is a separate, unidentified engine flag.
    float2 offset = float2(-f16tof32(p >> 16) * (float(width) * 0.5),
                           f16tof32(p & 0xfffe) * (float(height) * 0.5));
    color_out[id.xy] = float4(color_in.Load(int3(id.xy, 0)), 1.0);
    depth_out[id.xy] = depth_in.Load(int3(id.xy, 0));
    motion_out[id.xy] = offset;
}
)";
}

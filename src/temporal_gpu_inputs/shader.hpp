#pragma once
namespace mhw::temporal_gpu_inputs {
// 0.32.0: DLSS reads the colour copy itself (R11G11B10); only depth (for frame
// generation's render-size staging) and motion are written. Motion is RG16F:
// the game stores it as two halves, so the scaled offsets keep its precision.
inline constexpr char shader_source[] = R"(
cbuffer Dimensions : register(b0) { uint width; uint height; };
Texture2D<float> depth_in : register(t0);
Texture2D<uint> motion_in : register(t1);
RWTexture2D<float> depth_out : register(u0);
RWTexture2D<float2> motion_out : register(u1);
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
    uint p = motion_in.Load(int3(id.xy, 0));
    // Verified MHW history offset: previous_pixel = current_pixel + offset.
    // The low bit of the lower half is a separate, unidentified engine flag.
    float2 offset = float2(-f16tof32(p >> 16) * (float(width) * 0.5),
                           f16tof32(p & 0xfffe) * (float(height) * 0.5));
    depth_out[id.xy] = depth_in.Load(int3(id.xy, 0));
    motion_out[id.xy] = offset;
}
)";
} // namespace mhw::temporal_gpu_inputs

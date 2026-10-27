# NVIDIA Image Scaling SDK 1.0.3 (NVSharpen)

From <https://github.com/NVIDIAGameWorks/NVIDIAImageScaling>, tag `v1.0.3`,
commit `35e13ba316c98eeecf16f37eae70ce88019911f6`. MIT license:
`LICENSE.txt` (the SDK's `licence.txt`).

- `NIS_Config.h`: unmodified. The mod computes NVSharpen's constants with
  `NVSharpenUpdateConfig` (`src/sharpen/sharpen_config.hpp`).
- `NIS_Scaler.h`: the shader, compiled at run time with `D3DCompile` and
  embedded in `mhw_dlss.dll` as text (`cmake/EmbedText.cmake`). One change:
  `NVTEX_SAMPLE` and `NVTEX_STORE` (HLSL) are defined only if the including
  shader has not defined them, so it encodes colour on load and decodes it on
  store (`docs/research/nvsharpen.md`). Upstream SHA-256 of the file:
  `2d3bd5b45d6a0c4169fe36dfe349e7ff5a099dff6fb6876f8a3286489f74d3bf`.

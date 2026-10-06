# DXBC container hash

Unmodified `DxilHash.cpp`, `DxilHash.h`, and `LICENSE.TXT` from Microsoft's
DirectXShaderCompiler, commit `69c311af7091953427a6d67c16e811ce65313b93`.
Source paths and SHA-256 identities are recorded in `upstream.json` and CMake.
License: University of Illinois/NCSA Open Source License; retain LICENSE.TXT
with redistributed sources and reproduce its notice in binary distributions.

Only the retail container checksum is used, when rebuilding an already
SHA-256-qualified shader in private memory. It is not a security hash. Native
shader identification uses BCrypt SHA-256 separately. No game shaders are
included in this directory or in the distributable adapter.

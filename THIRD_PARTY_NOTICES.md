# Third-party notices

## MinHook

Source: <https://github.com/TsudaKageyu/minhook>

Pinned revision: `c3fcafdc10146beb5919319d0683e44e3c30d537` (v1.3.4)

```text
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.

Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## NVIDIA Streamline public headers/source

Source: <https://github.com/NVIDIA-RTX/Streamline>

Pinned version: 2.12.0, commit `e8aaa6eaac968711fb62473d4ae8256dde20919b`.
Public `license.txt` SHA256:
`7b6f23e7d6f3ad6292f9308d2b42cdc3d82ae4e9b2abb55f230279d83bedd43d`.

```text
Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The source license excludes `sl_nvperf.h` and `sl_nvperf.dll`, which have a
separate Nsight Perf SDK license; this integration does not use them.

Public-source licensing does not replace NVIDIA runtime/SDK licenses. Local
research dependencies record `reflex.license.txt` SHA256
`ebf83c07fb3b2939908c3795d887afde3161c89a28ba391724efc784ce1bdabe`
and `nvngx_dlss.license.txt` SHA256
`3027f23ca5a46dd9cb8183fbd522983a86f64d7daac5982912bf9f214671f294`.
NVIDIA's runtime DLLs are never committed. Release packages
(`tools/package_release.py`) ship the unmodified DLLs the build was tested
with, verified by SHA-256, together with NVIDIA's license files and the
attribution notice (`AGENTS.md`).

## NVIDIA DLSS SDK

Source: <https://github.com/NVIDIA/DLSS>, commit
`a291cc7d2cc642a51566f3dfd5376f635cd1b284` (`cmake/NgxReplay.cmake` pins the
files by SHA-256). The mod compiles against its headers and links its NGX
library `nvsdk_ngx_d.lib` into `mhw_dlss.dll`. License: NVIDIA RTX SDKs
License, the SDK's `LICENSE.txt` (shipped in release packages as
`NVIDIA-RTX-SDKs-LICENSE-DLSS.txt`).

## NVIDIA NVAPI declarations

`third_party/nvapi_sleep_status.hpp` declares a public ABI subset adapted from
NVIDIA/nvapi commit `70d337db9186e968eab622f7e786de7e437faf3d`. MIT license:
`third_party/nvapi-sleep-status-LICENSE.txt`. No NVAPI runtime is distributed.

## DirectXShaderCompiler DxilHash

Unmodified `DxilHash.cpp` and `DxilHash.h` from Microsoft's
DirectXShaderCompiler, commit `69c311af7091953427a6d67c16e811ce65313b93`
(`third_party/dxc_hash`). University of Illinois/NCSA Open Source License:
`third_party/dxc_hash/LICENSE.TXT`.

## Astera Loader and CMFM headers

`third_party/al/al_plugin.h` (Astera Loader's plugin API) and
`third_party/cmfm/cmfm.h` (CMFM's menu interface) are copied unchanged from
those projects; their folders' README files record the commits. MIT licenses:
`third_party/al/LICENSE` and `third_party/cmfm/LICENSE`.

## In release packages

`tools/package_release.py` lists every component above that is compiled into
`mhw_dlss.dll` in the package's `THIRD-PARTY-NOTICES.txt` and copies its license
file into the package's `licenses` folder. The MinHook license is
`third_party/minhook-LICENSE.txt`, copied unchanged from the pinned commit.

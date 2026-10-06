"""Disassemble one private captured SM5 compute shader using the Windows runtime."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
from temporal_shader import compute_dxbc, require

def disassemble(data):
    compute_dxbc(data)
    # Restrict DLL resolution to System32; no DLLs from the input directory.
    dll = c.WinDLL('d3dcompiler_47.dll', winmode=0x800)
    dll.D3DDisassemble.argtypes = [c.c_void_p, c.c_size_t, c.c_uint, c.c_char_p, c.POINTER(c.c_void_p)]
    dll.D3DDisassemble.restype = c.c_long
    memory = c.create_string_buffer(data)
    blob = c.c_void_p()
    result = dll.D3DDisassemble(memory, len(data), 0, None, c.byref(blob))
    require(result >= 0 and blob.value, f'D3DDisassemble HRESULT {result:#x}')
    table = c.cast(blob, c.POINTER(c.POINTER(c.c_void_p))).contents
    release = c.WINFUNCTYPE(c.c_ulong, c.c_void_p)(table[2])
    try:
        pointer = c.WINFUNCTYPE(c.c_void_p, c.c_void_p)(table[3])(blob)
        size = c.WINFUNCTYPE(c.c_size_t, c.c_void_p)(table[4])(blob)
        require(pointer and 0 < size <= 16 * 1024 * 1024, 'disassembly bounds')
        return c.string_at(pointer, size).rstrip(b'\0').decode('utf-8')
    finally:
        release(blob)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('shader', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    require(not args.output.exists(), 'preserve disassembly')
    require(args.shader.stat().st_size <= 262144, 'shader budget')
    data = args.shader.read_bytes()
    result = disassemble(data)
    with args.output.open('x', encoding='utf-8') as out:
        out.write(result)
    print(json.dumps(dict(shader_sha256=hashlib.sha256(data).hexdigest(), shader_bytes=len(data),
                          disassembly=str(args.output), gpu_commands=0), indent=2))

if __name__ == '__main__':
    main()

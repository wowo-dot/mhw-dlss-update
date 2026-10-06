"""Save v0.15 diagnostic channels through FILE_MAP_READ, with no process writes.

The game must remain open until the immutable capture has been saved. This
reader verifies transport identity/checksums, not game-code or GPU correctness.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import struct

from review_submission import decode_journal, fnv32, require, unique_object


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()
    require(0 < args.pid <= 0xFFFFFFFF, "invalid process ID")
    require(args.output_directory.is_dir(), "create the evidence directory first")
    k = c.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; k.OpenProcess.restype = w.HANDLE
    k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4; k.GetProcessTimes.restype = w.BOOL
    k.OpenFileMappingW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]; k.OpenFileMappingW.restype = w.HANDLE
    k.MapViewOfFile.argtypes = [w.HANDLE, w.DWORD, w.DWORD, w.DWORD, c.c_size_t]; k.MapViewOfFile.restype = c.c_void_p
    k.UnmapViewOfFile.argtypes = [c.c_void_p]; k.UnmapViewOfFile.restype = w.BOOL
    k.CloseHandle.argtypes = [w.HANDLE]; k.CloseHandle.restype = w.BOOL
    handle = k.OpenProcess(0x1000, False, args.pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not handle: raise c.WinError(c.get_last_error())
    try:
        times = [w.FILETIME() for _ in range(4)]
        if not k.GetProcessTimes(handle, *[c.byref(t) for t in times]): raise c.WinError(c.get_last_error())
        creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
    finally:
        k.CloseHandle(handle)

    def mapping(name, size, magic, schema):
        handle = k.OpenFileMappingW(4, False, name)
        if not handle: raise c.WinError(c.get_last_error())
        view = None
        try:
            view = k.MapViewOfFile(handle, 4, 0, 0, size)
            if not view: raise c.WinError(c.get_last_error())
            first = c.string_at(view, size)
            require(first == c.string_at(view, size), "mapping is still changing")
            require(struct.unpack_from("<QIIIIQ", first) == (magic, schema, 1500, size, args.pid, creation), "mapping identity")
            return first
        finally:
            if view: k.UnmapViewOfFile(view)
            k.CloseHandle(handle)

    mailbox = mapping(f"Local\\MhwDlssBridgeCapture-v1-{args.pid}-{creation}", 131184, 0x315041434457484D, 1)
    state, length, checksum, index = struct.unpack_from("<4I", mailbox, 32)
    require(state == 4 and index == 0 and 2 <= length <= 131072, "Evaluate capture has not been published")
    payload, tail = mailbox[48:48 + length], mailbox[131120:131184]
    require(fnv32(payload + tail) == checksum, "capture checksum")
    capture = json.loads(payload, object_pairs_hook=unique_object)
    require(capture["schema"] == 1 and capture["producer"] == 1500 and capture["evaluate_index"] == 0, "capture identity")
    qualified, inputs = struct.unpack_from("<II", tail)
    binding = struct.unpack_from("<6Q2I", tail, 8)
    require(qualified in (0, 1) and inputs in (0, 1) and binding[-1] == 0, "capture binding")
    journal = mapping(f"Local\\MhwDlssBarrierJournal-v2-{args.pid}-{creation}", 753944, 0x314C4E4A4457484D, 2)
    decoded = decode_journal(journal)
    require(decoded["drained"], "journal has not drained; leave the game open")

    def save(name, data):
        with (args.output_directory / name).open("xb") as output:
            output.write(data)
        return dict(name=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper())

    report = dict(pid=args.pid, creation=creation, mapping_access="FILE_MAP_READ", writes=0, debugger=False,
                  copies_stable=True, capture_checksum_verified=True, qualified=bool(qualified), inputs_verified=bool(inputs),
                  binding=dict(command=binding[0], resources=list(binding[1:6]), thread=binding[6], index=binding[7]),
                  capture=save(f"capture-{args.pid}-0.json", payload + b"\n"),
                  journal=save(f"journal-{args.pid}.bin", journal), journal_header_verified=True,
                  journal_drained=True, termination=decoded["termination"], gpu_safe=False)
    save(f"channels-{args.pid}.json", (json.dumps(report, indent=2) + "\n").encode())
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

"""Signal the bounded native render-resolution probe for one process generation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import math
from pathlib import Path
import re

MAX_BYTES = 256 * 1024
HOST = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea"


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def parse(raw):
    require(0 < len(raw) <= MAX_BYTES, "log size")
    rows = [json.loads(line) for line in raw.decode("utf-8").splitlines() if line.strip()]
    require(rows and all(isinstance(r, dict) and "event" in r for r in rows), "records")
    h = rows[0]
    require(h.get("event") == "start" and h.get("schema") == 1 and h.get("version") == "1.22.1", "header")
    require(h.get("experiment") == "native-render-resolution", "experiment")
    require(h.get("host_sha256") == HOST and h.get("host_version") == "0.0.0.0", "host identity")
    require(h.get("graphics_api") == "D3D12" and h.get("dlss_evaluation") is False, "scope")
    require(h.get("runtime_limit_ms") == 300000, "bounded run")
    require(all(type(h.get(k)) is int and h[k] > 0 for k in ("pid", "creation", "base")), "process identity")
    require(re.fullmatch(r"[0-9a-f]{64}", h.get("dll_sha256", "")), "DLL hash")
    scale = h.get("requested_scale")
    require(type(scale) in (int, float) and math.isfinite(scale) and .5 <= scale < 1, "requested scale")
    return rows


def validate(rows, pid, creation, expected_hash, stop=False):
    h = rows[0]
    require(h["pid"] == pid and h["creation"] == creation, "process generation")
    require(h["dll_sha256"] == expected_hash.lower(), "installed DLL identity")
    require(not any(r["event"] in ("failed", "stopped") for r in rows), "recorder finished")
    if not stop:
        require([r["event"] for r in rows] == ["start", "reservation", "ready"], "readiness sequence")
        require(rows[1].get("ready") is True and rows[1].get("slot", 0) > 0, "startup reservation")
        require(rows[1].get("hooks_installed") is False and rows[2].get("hooks_installed") is False, "unarmed readiness")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", type=Path, required=True)
    ap.add_argument("--dll-sha256", required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--stop", action="store_true")
    a = ap.parse_args()
    require(not a.output.exists(), "preserve receipt")
    require(a.log.stat().st_size <= MAX_BYTES, "log size")
    raw = a.log.read_bytes()
    rows = parse(raw)
    pid = rows[0]["pid"]
    k = c.WinDLL("kernel32", use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; k.OpenProcess.restype = w.HANDLE
    k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4; k.GetProcessTimes.restype = w.BOOL
    k.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]; k.OpenEventW.restype = w.HANDLE
    k.SetEvent.argtypes = [w.HANDLE]; k.SetEvent.restype = w.BOOL
    k.CloseHandle.argtypes = [w.HANDLE]; k.GetTickCount64.restype = c.c_uint64
    process = k.OpenProcess(0x1000, False, pid)
    require(process, "process unavailable")
    try:
        times = [w.FILETIME() for _ in range(4)]
        require(k.GetProcessTimes(process, *map(c.byref, times)), "process times")
        creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        require(not (times[1].dwHighDateTime or times[1].dwLowDateTime), "process exited")
        validate(rows, pid, creation, a.dll_sha256, a.stop)
        event = k.OpenEventW(2, False, f'Local\\MhwRenderResolution-v1-{pid}-{creation}-{0 if a.stop else 1}')
        require(event, "event unavailable")
        try:
            require(k.SetEvent(event), "signal failed")
            tick = k.GetTickCount64()
        finally:
            k.CloseHandle(event)
    finally:
        k.CloseHandle(process)
    receipt = dict(pid=pid, creation=creation, requested_tick=tick, stop=a.stop,
                   request_sent=True, acknowledged=False, process_memory_writes=0,
                   log_sha256_at_request=hashlib.sha256(raw).hexdigest())
    with a.output.open("x") as out:
        json.dump(receipt, out, indent=2)
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()

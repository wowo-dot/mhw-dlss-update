"""Signal the independent-AA queue observer for one exact process generation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path

HOST = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea"


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def parse(raw, owned=False, deferred=False, gpu=False, pixels=False, projection=False, sdk=False):
    projection = projection or sdk
    pixels = pixels or projection
    gpu = gpu or pixels
    deferred = deferred or gpu
    owned = owned or deferred
    require(0 < len(raw) <= (4194304 if gpu else 1048576), "log size")
    rows = [json.loads(line) for line in raw.decode("utf-8").splitlines() if line.strip()]
    require(rows and all(isinstance(row, dict) for row in rows), "records")
    header = rows[0]
    versions = ("0.7.0",) if sdk else ("0.6.0","0.6.1") if projection else ("0.5.0" if pixels else "0.4.0" if gpu else "0.3.0" if deferred else "0.2.0" if owned else "0.1.0",)
    require(header.get("event") == "start" and header.get("schema") == 1 and header.get("version") in versions, "header")
    require(header.get("host_sha256") == HOST and header.get("host_version") == "0.0.0.0", "host identity")
    require(header.get("experiment") == ("independent-aa-sdk" if sdk else "independent-aa-projection-bindings" if projection else "independent-aa-input-readback" if pixels else "independent-aa-gpu-trace" if gpu else "independent-aa-deferred-marker" if deferred else "independent-aa-owned-in-place" if owned else "independent-aa-post-queue-boundary") and header.get("sample_limit") == 8, "experiment")
    if gpu:
        require(header.get('private_queue_signals') == 8 and header.get('gpu_state_trace') is True, 'GPU trace scope')
    require(header.get("private_gpu_commands") == (-1 if sdk else 27 if pixels else 0) and header.get("sdk_used") is sdk, "observer scope")
    if pixels:
        require(header.get('readback_receipts') == [0,3,7] and header.get('read_point') == 'first-paired-transition-after-marker', 'readback scope')
    if owned:
        require(header.get("write_scope") == ("scoped-post-queue-and-one-empty-packet" if deferred else "scoped-post-queue-only") and header.get("aa_option_writes") == 0 and header.get("jitter_writes") == (12 if projection else 0), "owned scope")
        require(header.get("pass_phase") == 15 and header.get("pass_order") == 2 and header.get("in_place") is True, "pass placement")
    else:
        require(header.get("game_data_writes") == 0, "read-only scope")
    if projection:
        require(header.get('projection_receipts') == header.get('binding_snapshots') == 8 and header.get('jitter_stimuli') == 6,'projection scope')
        if header['version'] in ('0.6.1','0.7.0'):require(header.get('shutdown_order')=='drain-before-disable-v1','shutdown order')
    if sdk:
        require(header.get('sdk_evaluations') == 8 and header.get('sdk_output') == 'private-only', 'SDK scope')
    require(all(type(header.get(key)) is int and header[key] > 0 for key in ("pid", "creation", "base")), "process identity")
    return rows


def validate(rows, pid, creation, dll_sha, stop=False):
    header = rows[0]
    require((header["pid"], header["creation"]) == (pid, creation), "process generation")
    require(header.get("dll_sha256") == dll_sha.lower(), "DLL identity")
    require(not any(row["event"] in ("stopped", "failed") for row in rows), "observer finished")
    if not stop:
        require([row["event"] for row in rows] == ["start", "ready"], "observer readiness")
        require(rows[1].get("reserved") is True and rows[1].get("hooks_installed") is False, "reservation")


def main(owned=False, deferred=False, gpu=False, pixels=False, projection=False, sdk=False):
    projection = projection or sdk
    pixels = pixels or projection
    gpu = gpu or pixels
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--dll-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stop", action="store_true")
    args = parser.parse_args()
    require(not args.output.exists(), "preserve receipt")
    require(args.log.stat().st_size <= (4194304 if gpu else 1048576), "log size")
    raw = args.log.read_bytes()
    rows = parse(raw, owned, deferred, gpu, pixels, projection, sdk)
    pid = rows[0]["pid"]
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4
    kernel.GetProcessTimes.restype = w.BOOL
    kernel.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
    kernel.OpenEventW.restype = w.HANDLE
    kernel.SetEvent.argtypes = [w.HANDLE]
    kernel.SetEvent.restype = w.BOOL
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.GetTickCount64.restype = c.c_uint64
    process = kernel.OpenProcess(0x1000, False, pid)
    require(process, "process unavailable")
    try:
        times = [w.FILETIME() for _ in range(4)]
        require(kernel.GetProcessTimes(process, *map(c.byref, times)), "process times")
        creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        require(not (times[1].dwHighDateTime or times[1].dwLowDateTime), "process exited")
        validate(rows, pid, creation, args.dll_sha256, args.stop)
        prefix = "MhwAaSdk-v1" if sdk else "MhwAaProjection-v1" if projection else "MhwAaInputs-v1" if pixels else "MhwAaGpu-v1" if gpu else "MhwAaDeferred-v1" if deferred else "MhwAaOwned-v1" if owned else "MhwAaBoundary-v1"
        event = kernel.OpenEventW(2, False, f"Local\\{prefix}-{pid}-{creation}-{0 if args.stop else 1}")
        require(event, "event unavailable")
        try:
            require(kernel.SetEvent(event), "signal failed")
            tick = kernel.GetTickCount64()
        finally:
            kernel.CloseHandle(event)
    finally:
        kernel.CloseHandle(process)
    result = dict(pid=pid, creation=creation, requested_tick=tick, stop=args.stop,
                  request_sent=True, acknowledged=False, process_memory_writes=0,
                  log_sha256_at_request=hashlib.sha256(raw).hexdigest())
    with args.output.open("x") as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

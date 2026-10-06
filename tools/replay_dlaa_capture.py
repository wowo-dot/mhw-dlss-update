"""Run the public NGX lifecycle control against a sealed live-owned archive.

Each saved frame gets a fresh process and reset history. This does not validate
temporal accumulation, jitter calibration, exposure, presentation, or gameplay.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(data)
    return digest.hexdigest()


def write(path: Path, data: dict) -> None:
    with path.open("x", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2, allow_nan=False)
        stream.write("\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, choices=(1, 3), default=3)
    args = parser.parse_args()
    archive_path = args.archive.resolve(strict=True)
    assert sha(archive_path) == args.archive_sha256
    archive = json.loads(archive_path.read_text(encoding="utf-8"))
    source = archive_path.parent
    names = set()
    for item in archive["files"]:
        path = (source / item["path"]).resolve(strict=True)
        assert path.is_relative_to(source) and item["path"] not in names
        names.add(item["path"])
        assert path.stat().st_size == item["bytes"] and sha(path) == item["sha256"], item["path"]
    assert archive["result"] in names
    result = json.loads((source / archive["result"]).read_text(encoding="utf-8"))
    assert result["review_file"] in names
    review = json.loads((source / result["review_file"]).read_text(encoding="utf-8"))
    assert review["accepted"] and result["all_gpu_outputs_match_independent_reference"]
    assert result["private_conversion_fences_completed"] and result["private_conversion_recordings_retired"]
    assert result["width"] == 2560 and result["height"] == 1440 and len(result["frames"]) == 3
    args.output.mkdir(parents=True, exist_ok=False)
    output = args.output.resolve()
    executable = args.executable.resolve(strict=True)
    sdk = args.sdk.resolve(strict=True)
    root = Path(__file__).resolve().parents[1]
    frozen = output / "source"
    for name in ("CMakeLists.txt", "cmake/NgxReplay.cmake", "src/dlss_sdk/contract.hpp",
                 "tools/dlaa_replay.cpp", "tests/dlss_sdk_contract_tests.cpp", "tools/replay_dlaa_capture.py"):
        target = frozen / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / name, target)
    shutil.copyfile(executable, output / "mhw_dlaa_replay.exe")
    executable = output / "mhw_dlaa_replay.exe"
    receipt = {
        "schema": 1, "experiment": "offscreen DLAA reset-per-frame public SDK lifecycle",
        "source_archive": archive_path.name, "source_archive_sha256": args.archive_sha256,
        "source_files_verified": len(names), "source_pid": result["pid"],
        "source_host_sha256": result["host_sha256"], "source_swapchain_format": result["swapchain_format"],
        "harness_sha256": sha(executable), "width": result["width"], "height": result["height"],
        "adapter_luid": result["adapter_luid"],
        "sdk_commit": "a291cc7d2cc642a51566f3dfd5376f635cd1b284",
        "sdk_runtime_sha256": sha(sdk / "lib/Windows_x86_64/rel/nvngx_dlss.dll"),
        "sdk_license_sha256": sha(sdk / "LICENSE.txt"),
        "game_process_access": False, "runs": [],
    }
    write(output / "preflight.json", receipt)
    all_ok = True
    for i, frame in enumerate(result["frames"][:args.frames]):
        prefix = f"inputs-{result['pid']}-{result['creation']}.owned-{i}"
        inputs = [source / f"{prefix}.{role}.f32" for role in ("color", "depth", "motion")]
        assert all(path.name in names for path in inputs)
        target = output / f"frame-{frame}"
        command = [str(executable), str(sdk / "lib/Windows_x86_64/rel"), str(target),
                   str(result["adapter_luid"][1]), str(result["adapter_luid"][0]),
                   str(result["width"]), str(result["height"]), *map(str, inputs)]
        started = time.monotonic()
        print(f"Evaluating saved frame {frame} with reset history", flush=True)
        try:
            completed = subprocess.run(command, capture_output=True, timeout=120, check=False)
            code, stdout, stderr = completed.returncode, completed.stdout, completed.stderr
        except subprocess.TimeoutExpired as exc:
            code, stdout, stderr = -1, exc.stdout or b"", exc.stderr or b""
        (output / f"stdout-{frame}.txt").write_bytes(stdout)
        (output / f"stderr-{frame}.txt").write_bytes(stderr)
        events_path = target / "events.jsonl"
        events = [json.loads(line) for line in events_path.read_text().splitlines()] if events_path.exists() else []
        success = bool(code == 0 and events and events[-1].get("event") == "success")
        entry = {"frame": frame, "seconds": time.monotonic() - started, "exit_code": code,
                 "success": success, "input_sha256": [sha(path) for path in inputs],
                 "final_event": events[-1] if events else None}
        receipt["runs"].append(entry)
        print(json.dumps(entry), flush=True)
        if not success:
            all_ok = False
            break
    receipt["offscreen_sdk_evaluation_verified"] = all_ok and len(receipt["runs"]) == args.frames
    receipt["temporal_quality_verified"] = receipt["game_integration_verified"] = False
    write(output / "result.json", receipt)
    if not receipt["offscreen_sdk_evaluation_verified"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()

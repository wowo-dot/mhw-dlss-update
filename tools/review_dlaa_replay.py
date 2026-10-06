"""Independently inspect offscreen SDK outputs; no temporal-quality verdict."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from replay_dlaa_capture import sha, write


def preview(rgb: np.ndarray) -> Image.Image:
    # A shared diagnostic display transform, not the game's tone mapper.
    linear = np.maximum(rgb, 0)
    linear = linear / (1 + linear)
    srgb = np.where(linear <= .0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - .055)
    return Image.fromarray(np.round(np.clip(srgb, 0, 1) * 255).astype(np.uint8), "RGB")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("replay", type=Path)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    receipt = json.loads((args.replay / "result.json").read_text())
    assert receipt["offscreen_sdk_evaluation_verified"]
    assert not receipt["temporal_quality_verified"] and not receipt["game_integration_verified"]
    source_manifest = args.source / receipt["source_archive"]
    assert sha(source_manifest) == receipt["source_archive_sha256"]
    manifest = json.loads(source_manifest.read_text())
    entries = {item["path"]: item for item in manifest["files"]}
    assert sha(args.replay / "mhw_dlaa_replay.exe") == receipt["harness_sha256"]
    source_result = json.loads((args.source / manifest["result"]).read_text())
    results = []
    width, height = receipt["width"], receipt["height"]
    for i, run in enumerate(receipt["runs"]):
        frame = run["frame"]
        assert run["success"] and run["exit_code"] == 0 and frame == source_result["frames"][i]
        native = args.replay / f"frame-{frame}"
        events = [json.loads(line) for line in (native / "events.jsonl").read_text().splitlines()]
        start, last = events[0], events[-1]
        assert start["event"] == "start" and start["reset"] and start["jitter"] == [0, 0]
        assert start["width"] == width and start["height"] == height and start["gpu_validation"]
        assert start["input_formats"] == [2, 41, 16] and start["output_format"] == 2
        assert start["runtime_sha256"] == receipt["sdk_runtime_sha256"]
        assert start["mode"] == 5 and start["preset"] == 11
        assert start["mv_scale"] == [1, 1] and start["depth_inverted"] and start["mv_low_res"]
        assert not start["mv_jittered"] and not start["temporal_accumulation_test"]
        assert last == run["final_event"] and last["cleanup_complete"] and last["event"] == "success"
        assert not any(e["event"] in ("failure", "d3d12_diagnostic") for e in events)
        apis = [e for e in events if e["event"] == "api"]
        assert [e["name"] for e in apis] == [
            "Init_with_ProjectID", "GetCapabilityParameters", "SuperSampling.Available",
            "NeedsUpdatedDriver", "FeatureInitResult", "CreateFeature", "EvaluateFeature",
            "ReleaseFeature", "DestroyParameters", "Shutdown1"]
        assert all(e["result"] == 1 for e in apis)
        completed = [e for e in events if e["event"] == "completed"]
        assert [e["phase"] for e in completed] == ["upload", "create", "evaluate"]
        assert [e["fence_value"] for e in completed] == [1, 2, 3]
        assert all(e["recording_retired"] for e in completed)
        assert [e for e in events if e["event"] == "debug_summary"] == [
            {"event": "debug_summary", "errors": 0, "warnings": 0}]
        prefix = f"inputs-{source_result['pid']}-{source_result['creation']}.owned-{i}"
        for j, role in enumerate(("color", "depth", "motion")):
            path = args.source / f"{prefix}.{role}.f32"
            assert sha(path) == run["input_sha256"][j] == entries[path.name]["sha256"]
        original = np.fromfile(args.source / f"{prefix}.color.f32", dtype="<f4").reshape(height, width, 4)
        out_path = native / "output.rgba32f"
        assert out_path.stat().st_size == width * height * 16 == last["output_bytes"]
        assert sha(out_path) == last["output_sha256"]
        output = np.fromfile(out_path, dtype="<f4").reshape(height, width, 4)
        rgb = output[..., :3]
        assert np.isfinite(rgb).all() and not np.any(rgb == 65504)
        assert float(rgb.std()) > 0
        changed = int(np.count_nonzero(rgb != original[..., :3]))
        assert changed == last["changed_rgb_values"]
        delta = np.abs(rgb.astype(np.float64) - original[..., :3])
        assert np.isclose(float(delta.mean()), last["rgb_mean_absolute_difference"], rtol=5e-6)
        record = {"frame": frame, "output_sha256": sha(out_path), "rgb_values_checked": rgb.size,
                  "all_rgb_finite": True, "all_rgb_overwritten": True,
                  "changed_rgb_values": changed, "mean_absolute_difference": float(delta.mean()),
                  "absolute_difference_quantiles": np.quantile(delta, [0, .5, .95, .99, 1]).tolist(),
                  "rgb_min": float(rgb.min()), "rgb_max": float(rgb.max()),
                  "rgb_mean": float(rgb.mean()), "rgb_std": float(rgb.std()),
                  "alpha_finite": bool(np.isfinite(output[..., 3]).all()),
                  "alpha_min": float(np.nanmin(output[..., 3])), "alpha_max": float(np.nanmax(output[..., 3])),
                  "rgb_correlation": float(np.corrcoef(original[..., :3].ravel(), rgb.ravel())[0, 1])}
        results.append(record)
        if i == 0:
            left, right = preview(original[..., :3]), preview(rgb)
            left.save(args.replay / "input-preview.png")
            right.save(args.replay / "dlaa-preview.png")
            pair = Image.new("RGB", (1600, 484), (22, 24, 28))
            pair.paste(left.resize((800, 450), Image.Resampling.LANCZOS), (0, 34))
            pair.paste(right.resize((800, 450), Image.Resampling.LANCZOS), (800, 34))
            draw = ImageDraw.Draw(pair)
            draw.text((12, 10), "Captured pre-TAA color | shared diagnostic tone mapping", fill="white")
            draw.text((812, 10), "NVIDIA DLAA output | history reset, zero jitter", fill="white")
            pair.save(args.replay / "comparison.png")
    write(args.replay / "verification.json", {
        "schema": 1, "offscreen_output_verified": True, "runs": results,
        "total_finite_rgb_values": sum(r["rgb_values_checked"] for r in results),
        "temporal_quality_verified": False, "game_integration_verified": False,
        "preview_transform": "max(RGB,0); Reinhard x/(1+x); sRGB; same transform both images",
        "alpha_excluded_from_rgb_validation": True})
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()

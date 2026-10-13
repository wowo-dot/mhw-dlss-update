"""Review native scale, observed viewports and restoration; no DLSS/output-quality claim."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
from arm_render_resolution import MAX_BYTES, parse, require


def f32(v):
    return struct.unpack("f", struct.pack("f", v))[0]


def geometry(display, scale):
    require(isinstance(display, list) and len(display) == 2 and
            all(type(n) is int and 64 <= n <= 16384 for n in display), "display extent")
    require(type(scale) in (int, float) and math.isfinite(scale) and .5 <= scale < 1, "scale")
    width = int(f32(f32(display[0]) * f32(scale))) & ~15
    quantized = f32(f32(width) / f32(display[0]))
    require(.5 <= quantized < 1, "quantized scale")
    return [int(f32(f32(n) * quantized)) for n in display], quantized


def snapshot(s, display, active=None):
    require(s.get("display") == display and s.get("view", 0) > 0 and s.get("tick", 0) > 0, "snapshot identity")
    require(type(s.get("frame")) is int and s["frame"] >= 0, "frame")
    values = s.get("scales")
    require(isinstance(values, list) and len(values) == 9 and
            all(type(v) in (int, float) and math.isfinite(v) for v in values), "snapshot scales")
    require(all(values[i] == 1 for i in (3, 4, 7)), "allocation scale changed")
    require(all(.5 <= values[i] <= 1 for i in (0, 1, 5, 6, 8)), "active range")
    require(not s.get("view_flags", 0) & 0x840000, "unqualified view")
    require(type(s.get("native_width_override")) is int and s["native_width_override"] in (0, 1) and
            s.get("native_scale_basis") == display, "native sizing basis mismatch")
    if active is not None:
        require(all(math.isclose(values[i], active, rel_tol=0, abs_tol=1e-7) for i in (1, 5, 6, 8)), "active scales disagree")
    require(s.get("depth_metadata") is True and s.get("depth_extent") == display, "depth CPU allocation metadata")
    return values


COUNTERS = ("visits", "unqualified", "overrides", "pre_restores", "foreign_requests", "scaled_observed", "restored_observed")


def review(rows, expected_hash, final=True):
    h = rows[0]
    require(h["dll_sha256"] == expected_hash.lower(), "DLL identity")
    events = [r["event"] for r in rows]
    require(events[:4] == ["start", "reservation", "ready", "armed"], "startup sequence")
    require(rows[1].get("ready") is True and rows[1].get("slot", 0) > 0 and
            rows[1].get("hooks_installed") is False and rows[2].get("hooks_installed") is False, "startup readiness")
    require(rows[3].get("installed") is True, "hook installation")
    require(all(e == "resolution" for e in events[4: (-1 if final else len(events))]), "capture sequence")
    if final:
        end = rows[-1]
        require(end.get("event") == "stopped" and end.get("armed") is True and
                end.get("healthy") is True and end.get("cleaned") is True and end.get("dll_pinned") is True and
                end.get("reason") in ("requested", "deadline"), "clean stop")
    samples = [r for r in rows if r["event"] == "resolution"]
    require(samples, "resolution samples")
    latest = samples[-1]
    render, quantized = geometry(latest["display"], h["requested_scale"])
    previous = dict.fromkeys(COUNTERS, 0)
    for s in samples:
        require(s.get("schema") == 1 and s.get("policy") == "native-active-scale-v1", "resolution policy")
        require(s.get("display") == latest["display"] and s.get("render") == render and
                s.get("requested_scale") == h["requested_scale"] and
                math.isclose(s.get("quantized_scale", -1), quantized, rel_tol=0, abs_tol=1e-7), "extent contract")
        require(s.get("fault") == 0, "runtime fault")
        require(s.get("dlss_evaluations") == s.get("injected_gpu_commands") == s.get("settings_writes") == 0, "scope changed")
        require(s.get("gpu_resource_descriptions_verified") is False and
                s.get("viewport_scene_attribution_verified") is False, "unsupported resource claim")
        for key in COUNTERS:
            require(type(s.get(key)) is int and s[key] >= previous[key], "counter regression: " + key)
            previous[key] = s[key]
    require(latest["overrides"] > 0 and latest["scaled_observed"] > 0 and latest.get("render_thread", 0) > 0, "scale not observed")
    require(latest.get("swapchain_format", 0) > 0 and len(latest.get("adapter_luid", [])) == 2, "adapter/swapchain identity")
    snapshot(latest["baseline"], latest["display"], 1)
    snapshot(latest["reduced"], latest["display"], quantized)
    require(latest["baseline"]["tick"] <= latest["reduced"]["tick"], "snapshot order")
    counts = latest.get("viewport_counts")
    require(isinstance(counts, list) and len(counts) == 3 and all(type(n) is int and n >= 0 for n in counts) and counts[0] > 0 and counts[1] > 0, "full/reduced viewports not observed")
    for key in ("first_reduced_viewport", "last_reduced_viewport"):
        v = latest[key]
        require(v.get("tick", 0) > 0 and v.get("caller", 0) > 0 and
                v.get("values", [])[:4] == [0, 0, *render], "viewport observation")
    if final:
        require(latest.get("hooks_disabled") is True and latest.get("restored") is True and
                latest.get("requested") is False and latest.get("owned_request") is False and latest.get("pending") == 0, "restoration incomplete")
        r = latest["restoration"]
        values = snapshot(r, latest["display"])
        require(all(values[i] == values[0] for i in (1, 5, 6, 8)), "native request not applied")
        require(r["tick"] >= latest["reduced"]["tick"] and latest["restored_observed"] > 0, "stale restoration")
        require(latest["pre_restores"] + latest["foreign_requests"] == latest["overrides"], "outstanding override")
    else:
        require(latest.get("requested") is True and latest.get("hooks_disabled") is False, "probe inactive")
    return dict(verified=True, final=final, pid=h["pid"], creation=h["creation"],
                display=latest["display"], render=render, quantized_scale=quantized,
                overrides=latest["overrides"], scaled_observed=latest["scaled_observed"],
                viewport_counts=counts, restoration_verified=final,
                resource_descriptions_verified=False, scene_attribution_verified=False,
                dlss_verified=False)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", type=Path, required=True)
    ap.add_argument("--dll-sha256", required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--checkpoint", action="store_true")
    a = ap.parse_args()
    require(not a.output.exists(), "preserve review")
    require(a.log.stat().st_size <= MAX_BYTES, "log size")
    raw = a.log.read_bytes()
    result = review(parse(raw), a.dll_sha256, final=not a.checkpoint)
    result["log_sha256"] = hashlib.sha256(raw).hexdigest()
    with a.output.open("x") as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

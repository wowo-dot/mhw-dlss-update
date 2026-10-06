import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import arm_render_resolution as arm
import review_render_resolution as review

DLL = "a" * 64


def fixture():
    def snap(tick, active, desired=1):
        return dict(tick=tick, frame=tick, view=1234, display=[2560, 1440],
                    scales=[desired, active, 1, 1, 1, active, active, 1, active],
                    view_flags=0, depth_metadata=True, depth_extent=[2560, 1440],
                    native_width_override=1, native_scale_basis=[2560,1440])
    h = dict(event="start", schema=1, version="1.22.1", experiment="native-render-resolution",
             host_sha256=arm.HOST, host_version="0.0.0.0", dll_sha256=DLL, graphics_api="D3D12",
             pid=55, creation=66, base=0x140000000, runtime_limit_ms=300000,
             requested_scale=.5, dlss_evaluation=False)
    s = dict(event="resolution", schema=1, policy="native-active-scale-v1", fault=0,
             requested=False, hooks_disabled=True, restored=True, owned_request=False, pending=0,
             display=[2560, 1440], render=[1280, 720], requested_scale=.5, quantized_scale=.5,
             visits=134, unqualified=0, overrides=100, pre_restores=100, foreign_requests=0,
             scaled_observed=99, restored_observed=1, render_thread=22,
             swapchain_format=28, adapter_luid=[0, 7], viewport_counts=[100, 300, 100],
             baseline=snap(1, 1), reduced=snap(2, .5), restoration=snap(3, 1),
             first_reduced_viewport=dict(tick=2, caller=42, values=[0, 0, 1280, 720, 0, 1]),
             last_reduced_viewport=dict(tick=3, caller=42, values=[0, 0, 1280, 720, 0, 1]),
             gpu_resource_descriptions_verified=False, viewport_scene_attribution_verified=False,
             dlss_evaluations=0, injected_gpu_commands=0, settings_writes=0)
    return [h, dict(event="reservation", ready=True, slot=123, hooks_installed=False),
            dict(event="ready", hooks_installed=False), dict(event="armed", installed=True), s,
            dict(event="stopped", armed=True, healthy=True, cleaned=True, reason="requested", dll_pinned=True)]


class ResolutionTest(unittest.TestCase):
    def test_verified_scope(self):
        rows = arm.parse(json.dumps(fixture()[0]).encode() + b"\n" +
                         b"\n".join(json.dumps(r).encode() for r in fixture()[1:]))
        result = review.review(rows, DLL)
        self.assertTrue(result["restoration_verified"])
        self.assertFalse(result["dlss_verified"])
        self.assertFalse(result["scene_attribution_verified"])

    def test_new_native_request_can_restore(self):
        rows = fixture()
        rows[4]["restoration"]["scales"] = [.75, .75, .5, 1, 1, .75, .75, 1, .75]
        self.assertTrue(review.review(rows, DLL)["verified"])

    def test_reject_invalid_evidence(self):
        mutations = [
            lambda r: r[0].update(dll_sha256="b" * 64),
            lambda r: r[4].update(render=[1280, 721]),
            lambda r: r[4].update(fault=4),
            lambda r: r[4].update(pre_restores=99),
            lambda r: r[4].update(viewport_counts=[100, 0, 100]),
            lambda r: r[4]["reduced"].update(scales=[1, .5, 1, .5, .5, .5, .5, .5, .5]),
            lambda r: r[4]["restoration"].update(tick=1),
            lambda r: r[4]["restoration"].update(scales=[1, .5, 1, 1, 1, .5, .5, 1, .5]),
            lambda r: r[4].update(owned_request=True),
            lambda r: r[4].update(pending=1),
            lambda r: r[4].update(hooks_disabled=False),
            lambda r: r[4].update(gpu_resource_descriptions_verified=True),
            lambda r: r[4]["reduced"].update(native_scale_basis=[1920,1080]),
            lambda r: r[4]["reduced"].update(native_width_override=2),
            lambda r: r[5].update(cleaned=False),
        ]
        for mutate in mutations:
            rows = fixture(); mutate(rows)
            with self.subTest(mutation=mutate), self.assertRaises(ValueError):
                review.review(rows, DLL)

    def test_counter_regression(self):
        rows = fixture()
        previous = copy.deepcopy(rows[4]); previous["overrides"] = 101
        rows.insert(4, previous)
        with self.assertRaises(ValueError): review.review(rows, DLL)

    def test_arm_generation_and_phase(self):
        ready = fixture()[:3]
        arm.validate(ready, 55, 66, DLL)
        for pid, creation, sha in ((56, 66, DLL), (55, 67, DLL), (55, 66, "b" * 64)):
            with self.assertRaises(ValueError): arm.validate(ready, pid, creation, sha)
        with self.assertRaises(ValueError): arm.validate(fixture(), 55, 66, DLL)

    def test_native_quantization(self):
        self.assertEqual(review.geometry([2560, 1440], 2/3)[0], [1696, 954])
        for dims, scale in (([65, 64], .5), ([2560, 1440], float("nan")), ([0, 1440], .5)):
            with self.assertRaises(ValueError): review.geometry(dims, scale)


if __name__ == "__main__":
    unittest.main()

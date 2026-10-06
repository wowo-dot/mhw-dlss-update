import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from review_frame_generation_fixture import validate


def sample(requested=2, direct=False):
    off = requested == 0
    rows = [
        dict(event='start', schema=1, version='0.1.8' if off else '0.3.2', sdk='2.12.0',
             graphics_api='D3D12', game_access=False, gpu_validation=True,
             fg_mode='Off' if off else 'On-fixed', requested_generated=requested,
             static_inputs=False, resource_tags=5, visible_window=True, direct_sdk=direct),
        dict(event='sdk_initialized', before_dxgi_device=True, features=['Reflex', 'PCL', 'DLSS_G']),
        dict(event='feature_support', result=0, requirements_result=0, vendor_id=0x10de,
             device_id=1, required_tags=4, max_viewports=1),
        dict(event='fg_capability', status=0, width=1280, height=720, swapchain_format=87,
             swapchain_flags=2112, minimum_dimension=100, max_generated_frames=5)]
    if not off:
        rows.append(dict(event='owned_inputs_ready', width=1280, height=720, depth_format=41,
                         motion_format=16, hudless_format=87, ui_alpha_format=61, resource_state=64,
                         static_camera=True, moving_object=True, ui_empty=True, initialized_gpu_complete=True))
    for frame in range(1, 145):
        if not off and frame in (33, 129):
            rows.append(dict(event='fg_mode', frame=frame, mode='On' if frame == 33 else 'Off',
                             generate=requested, foreground=True))
        rows.append(dict(event='fg_off_frame' if off else 'fg_frame', frame=frame, token_id=frame,
                         actual_presentations=1 + (requested if 33 <= frame <= 128 else 0),
                         status=0, foreground=True))
        if off and frame in (48, 96):
            rows.append(dict(event='resize', frame=frame, verified=True))
    for event in ('gpu_diagnostics', 'dxgi_diagnostics'):
        rows.append(dict(event=event, phase='before_shutdown', warnings=0, errors=0))
    rows.extend(dict(event=e) for e in ('sdk_shutdown_begin', 'sdk_shutdown_complete', 'pins_released',
                                      'native_post_stop_present_complete'))
    for event in ('gpu_diagnostics', 'dxgi_diagnostics'):
        rows.append(dict(event=event, phase='after_swap_destruction', warnings=0, errors=0))
    rows.append(dict(event='finished', host_frames=144, reflex_frames=144, tokens=144, sleeps=144,
                     markers=864, generated_frames=requested * 96,
                     sdk_reported_presentations=144 + requested * 96, faults=0, resizes=2 if off else 0,
                     device_refs=0, debug_warnings=0, debug_errors=0, accepted=True,
                     driver_on_verified=True, driver_off_verified=True, sdk_shutdown=True, pins_released=True,
                     native_after_stop=True, native_chain_recreated=True, hooks_restored=12 if off else True,
                     installed_hooks_during_test=0 if direct else 12, sdk_paced_host_calls=0 if direct else 144,
                     focused_on_frames=96, input_fence_waits=112, requested_generated=requested,
                     multiplier=requested + 1, full_multiplier_frames=96))
    return rows


class Review(unittest.TestCase):
    def test_off_and_fixed_modes(self):
        for requested in (0, 1, 2, 3, 5):
            with self.subTest(requested=requested):
                result = validate(sample(requested))
                self.assertEqual(result['reported_extra_presentations'], requested * 96)
                self.assertFalse(result['game_fg_implemented'])

    def test_direct_control(self):
        self.assertTrue(validate(sample(1, True))['direct_sdk'])

    def test_api_success_without_extra_presentations_is_rejected(self):
        rows = sample(1)
        for row in rows:
            if row['event'] == 'fg_frame':
                row['actual_presentations'] = 1
        with self.assertRaisesRegex(ValueError, 'actual fixed-multiplier'):
            validate(rows)

    def test_negative_evidence(self):
        cases = [('start', 'game_access', True), ('start', 'sdk', 'different'),
                 ('sdk_initialized', 'before_dxgi_device', False), ('feature_support', 'result', 1),
                 ('fg_capability', 'max_generated_frames', 1), ('owned_inputs_ready', 'initialized_gpu_complete', False),
                 ('owned_inputs_ready', 'hudless_format', 0), ('owned_inputs_ready', 'resource_state', 8), ('fg_mode', 'generate', 3),
                 ('finished', 'device_refs', 1), ('finished', 'accepted', False),
                 ('finished', 'tokens', 143), ('finished', 'markers', 863),
                 ('finished', 'sdk_reported_presentations', 144), ('finished', 'hooks_restored', 12),
                 ('finished', 'driver_off_verified', False), ('finished', 'native_chain_recreated', False),
                 ('gpu_diagnostics', 'errors', 1), ('dxgi_diagnostics', 'warnings', 1)]
        for event, key, value in cases:
            with self.subTest(event=event, key=key):
                rows = sample()
                next(r for r in rows if r['event'] == event)[key] = value
                with self.assertRaises(ValueError):
                    validate(rows)
        for key, value in [('token_id', 34), ('frame', 34), ('status', 1), ('foreground', False)]:
            with self.subTest(enabled_frame=key):
                rows = sample()
                next(r for r in rows if r['event'] == 'fg_frame' and r['frame'] == 33)[key] = value
                with self.assertRaises(ValueError):
                    validate(rows)

    def test_incomplete_and_reordered_lifetimes(self):
        rows = sample()
        for changed in (rows[:-1], rows + [copy.deepcopy(rows[-1])],
                        rows[:4] + [dict(event='failure', reason='worker failure')] + rows[4:]):
            with self.assertRaises(ValueError):
                validate(changed)
        rows = sample()
        a = next(i for i, r in enumerate(rows) if r['event'] == 'pins_released')
        b = next(i for i, r in enumerate(rows) if r['event'] == 'sdk_shutdown_complete')
        rows[a], rows[b] = rows[b], rows[a]
        with self.assertRaises(ValueError):
            validate(rows)


if __name__ == '__main__':
    unittest.main()

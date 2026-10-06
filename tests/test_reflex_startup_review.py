import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from review_reflex_startup import validate, HOST_SHA

def sample():
    return [
        dict(event='start', schema=1, version='0.1.3', experiment='reflex-startup-device', host_sha256=HOST_SHA,
             graphics_api='D3D12', sdk='2.12.0', mode=0, dll_sha256='a'*64, sleep=False, markers=False, upscaling=False),
        dict(event='api_module', kind=0, was_loaded=False, loaded_from_system32=True, module=100, target=2000),
        dict(event='api_module', kind=1, was_loaded=False, loaded_from_system32=True, module=200, target=3000),
        dict(event='prepared', queued_api_hooks=2, sdk_initialized=False, original_heads_rva=4000, targets=[2000, 3000]),
        dict(event='sdk_initialized', before_factory=False, after_native_device_creation=True,
             before_native_queues_and_swapchain=True, caller_rva=0x23a4e2e, device=1000, thread=7, sleep=False, markers=False),
        dict(event='device_checked', caller_rva=0x23a4e2e, thread=7, device=1000, low_latency_available=True,
             mode=0, rendering_modified=False, sleep_calls=0, markers=0),
        dict(event='sdk_shutdown', passed=True, before_native_device_use_resumes=True),
        dict(event='hook_restoration', kind=0, restored=True, target=2000, original_head_sha256='b'*64),
        dict(event='hook_restoration', kind=1, restored=True, target=3000, original_head_sha256='c'*64),
        dict(event='swapchain_snapshot', native_interface=True, width=2560, height=1440, format=87, buffers=3, windowed=True),
        dict(event='finished', startup_passed=True, hooks_restored=True, sdk_shutdown_passed=True,
             swapchain_observed=True, qualified_factories=0, qualified_devices=1, pending_calls=0,
             swapchain_proxy_installed=False, game_reflex_active=False)]

class Review(unittest.TestCase):
    def test_complete(self):
        result = validate(sample())
        self.assertTrue(result['startup_passed'])
        self.assertFalse(result['game_reflex_active'])
    def test_rejections(self):
        for index, key, value in [(0, 'upscaling', True), (4, 'before_factory', 'false'),
            (5, 'thread', 8), (6, 'passed', False), (7, 'restored', False),
            (10, 'pending_calls', 1), (10, 'game_reflex_active', True), (9, 'buffers', 0),
            (1, 'loaded_from_system32', False), (2, 'was_loaded', 'false'),
            (3, 'targets', [3000, 2000]), (3, 'original_heads_rva', 0),
            (7, 'target', 4000), (0, 'version', '0.1.2'), (4,'after_native_device_creation',False),
            (4,'before_native_queues_and_swapchain',False), (4,'device',999), (10,'qualified_factories',-1)]:
            with self.subTest(key=key):
                rows = copy.deepcopy(sample()); rows[index][key] = value
                with self.assertRaises(ValueError): validate(rows)
    def test_missing_or_duplicate(self):
        for rows in (sample()[:-1], sample()+[sample()[6]], sample()+[dict(event='failed')],
                     sample()[:1] + sample()[2:]):
            with self.assertRaises(ValueError): validate(rows)

if __name__ == '__main__':
    unittest.main()

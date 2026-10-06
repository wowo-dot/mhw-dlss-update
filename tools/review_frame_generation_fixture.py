"""Review owned FG/MFG execution evidence; never accesses a running process."""
from pathlib import Path
import argparse
import hashlib
import json


def validate(rows):
    def check(ok, reason):
        if not ok:
            raise ValueError(reason)

    def one(event):
        found = [r for r in rows if r.get('event') == event]
        check(len(found) == 1, event + ': expected one record')
        return found[0]

    check(rows and rows[0].get('event') == 'start' and rows[-1].get('event') == 'finished', 'complete trace')
    check(not any(r.get('event') in ('failure', 'failure_cleanup') for r in rows), 'failure recorded')
    start, init, support, caps, finish = [one(e) for e in ('start', 'sdk_initialized', 'feature_support', 'fg_capability', 'finished')]
    off = start.get('fg_mode') == 'Off'
    check(start.get('schema') == 1 and start.get('version') in ('0.1.8', '0.3.2'), 'fixture schema/version')
    check(start.get('sdk') == '2.12.0' and start.get('graphics_api') == 'D3D12' and
          start.get('game_access') is False and start.get('gpu_validation') is True, 'owned SDK/API/validation contract')
    check(init.get('before_dxgi_device') is True and init.get('features') == ['Reflex', 'PCL', 'DLSS_G'], 'early FG initialization')
    check(support.get('result') == 0 and support.get('requirements_result') == 0 and
          support.get('vendor_id') == 0x10de and support.get('device_id', 0) > 0 and
          support.get('required_tags') == 4 and support.get('max_viewports') == 1, 'supported adapter/requirements')
    check(rows.index(start) < rows.index(init) < rows.index(support) < rows.index(caps), 'initialization order')
    check(caps.get('status') == 0 and caps.get('width') == 1280 and caps.get('height') == 720 and
          caps.get('swapchain_format') == 87 and caps.get('swapchain_flags') == 2112 and
          0 < caps.get('minimum_dimension', 0) <= 720, 'qualified swap chain')
    requested = 0 if off else start.get('requested_generated', 1)
    check(type(requested) is int and requested in (0, 1, 2, 3, 5) and
          caps.get('max_generated_frames', 0) >= max(1, requested), 'fixed multiplier within capability')
    if not off:
        check(start.get('fg_mode') in ('On-2x', 'On-fixed') and start.get('static_inputs') is False and
              start.get('resource_tags') == 5 and start.get('visible_window') is True and
              type(start.get('direct_sdk')) is bool, 'owned moving inputs')
        inputs = one('owned_inputs_ready')
        for key, value in dict(width=1280, height=720, depth_format=41, motion_format=16,
                               hudless_format=87, ui_alpha_format=61, resource_state=64,
                               static_camera=True, moving_object=True, ui_empty=True,
                               initialized_gpu_complete=True).items():
            check(type(inputs.get(key)) is type(value) and inputs[key] == value, 'input ' + key)
        check(rows.index(caps) < rows.index(inputs), 'input initialization order')
    frames = [r for r in rows if r.get('event') == ('fg_off_frame' if off else 'fg_frame')]
    check(len(frames) == 144, '144 complete host observations')
    reported = extra = 0
    for n, frame in enumerate(frames, 1):
        check(type(frame.get('frame')) is int and frame['frame'] == n and
              type(frame.get('token_id')) is int and frame['token_id'] == n, 'ordered same-frame token')
        check(frame.get('status') == 0, 'FG runtime status')
        enabled = not off and 33 <= n <= 128
        expected = requested + 1 if enabled else 1
        check(type(frame.get('actual_presentations')) is int and frame['actual_presentations'] == expected,
              'actual fixed-multiplier execution')
        if enabled:
            check(frame.get('foreground') is True, 'enabled observations must be foreground')
            extra += frame['actual_presentations'] - 1
        reported += frame['actual_presentations']
    if not off:
        modes = [r for r in rows if r.get('event') == 'fg_mode']
        check(len(modes) == 2 and modes[0].get('frame') == 33 and modes[0].get('mode') == 'On' and
              modes[0].get('generate') == requested and modes[0].get('foreground') is True and
              modes[1].get('frame') == 129 and modes[1].get('mode') == 'Off', 'On/Off sequence')
        check(rows.index(frames[31]) < rows.index(modes[0]) < rows.index(frames[32]) and
              rows.index(frames[127]) < rows.index(modes[1]) < rows.index(frames[128]), 'mode frame association')
        check(finish.get('focused_on_frames') == 96 and finish.get('input_fence_waits', 0) > 0, 'foreground/input consumption')
        if start['version'] == '0.3.2':
            check(finish.get('requested_generated') == requested and finish.get('multiplier') == requested + 1 and
                  finish.get('full_multiplier_frames') == 96, 'fixed MFG receipt')
    for key, value in dict(host_frames=144, reflex_frames=144, tokens=144, sleeps=144, markers=864,
                           generated_frames=extra, sdk_reported_presentations=reported,
                           faults=0, resizes=2 if off else 0, device_refs=0, debug_warnings=0, debug_errors=0).items():
        check(type(finish.get(key)) is int and finish[key] == value, 'completion ' + key)
    for key in ('accepted', 'driver_on_verified', 'driver_off_verified', 'sdk_shutdown', 'pins_released',
                'native_after_stop', 'native_chain_recreated'):
        check(finish.get(key) is True, 'completion ' + key)
    if off:
        check(type(finish.get('hooks_restored')) is int and finish['hooks_restored'] == 12, 'Off restored head count')
        sizes = [r for r in rows if r.get('event') == 'resize']
        check(len(sizes) == 2 and [r.get('frame') for r in sizes] == [48, 96] and
              all(r.get('verified') is True for r in sizes), 'qualified resize sequence')
    else:
        check(finish.get('hooks_restored') is True and
              finish.get('installed_hooks_during_test') == (0 if start['direct_sdk'] else 12), 'On hook restoration')
    check(finish.get('sdk_paced_host_calls') == (0 if start.get('direct_sdk') else 144), 'routed host transaction count')
    shutdown_begin, shutdown_end, pins, native = [one(e) for e in
        ('sdk_shutdown_begin', 'sdk_shutdown_complete', 'pins_released', 'native_post_stop_present_complete')]
    check(rows.index(frames[-1]) < rows.index(shutdown_begin) < rows.index(shutdown_end) < rows.index(pins) < rows.index(native) < rows.index(finish), 'quiescent native return')
    for event in ('gpu_diagnostics', 'dxgi_diagnostics'):
        samples = [r for r in rows if r.get('event') == event]
        check(len(samples) == 2 and {r.get('phase') for r in samples} == {'before_shutdown', 'after_swap_destruction'} and
              all(r.get('warnings') == 0 and r.get('errors') == 0 for r in samples), event + ': clean graphics validation')
        check(rows.index(samples[0]) < rows.index(shutdown_begin) and rows.index(native) < rows.index(samples[1]) < rows.index(finish), event + ': lifetime coverage')
    return dict(schema=1, accepted=True, game_access=False, game_fg_implemented=False, sdk='2.12.0',
                requested_generated=requested, multiplier=requested + 1, host_frames=144,
                sdk_reported_presentations=reported, reported_extra_presentations=extra,
                native_return_verified=True, device_refs=0, direct_sdk=start.get('direct_sdk', False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    raw = args.log.read_bytes()
    result = validate([json.loads(line) for line in raw.splitlines()])
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    data = json.dumps(result, indent=2) + '\n'
    if args.output:
        with args.output.open('x', encoding='utf-8') as out:
            out.write(data)
    print(data, end='')


if __name__ == '__main__':
    main()

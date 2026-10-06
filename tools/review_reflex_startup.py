"""Review the one-shot Reflex startup/device test; never opens the process."""
from pathlib import Path
import argparse
import hashlib
import json
import re

HOST_SHA = 'c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'

def check(value, reason):
    if not value:
        raise ValueError(reason)

def validate(rows):
    check(bool(rows), 'empty trace')
    def one(event):
        matches = [r for r in rows if r.get('event') == event]
        check(len(matches) == 1, event + ': expected one record')
        return matches[0]
    start, prepared, init, device, shutdown, swap, finish = [one(e) for e in
        ('start', 'prepared', 'sdk_initialized', 'device_checked', 'sdk_shutdown', 'swapchain_snapshot', 'finished')]
    check(start.get('schema') == 1 and start.get('version') == '0.1.3' and
          start.get('experiment') == 'reflex-startup-device', 'trace version')
    check(start.get('host_sha256') == HOST_SHA and start.get('graphics_api') == 'D3D12' and
          start.get('sdk') == '2.12.0' and start.get('mode') == 0, 'host/API/SDK/mode')
    check(isinstance(start.get('dll_sha256'), str) and bool(re.fullmatch('[0-9a-f]{64}', start['dll_sha256'])), 'probe hash')
    for key in ('sleep', 'markers', 'upscaling'):
        check(start.get(key) is False, key + ' must be disabled')
    check(not any(r.get('event') in ('failed', 'worker_failed') for r in rows), 'startup failure recorded')
    check(rows.index(start) < rows.index(prepared) < rows.index(init) < rows.index(device) < rows.index(shutdown) < rows.index(swap) < rows.index(finish), 'startup sequence')
    modules = [r for r in rows if r.get('event') == 'api_module']
    check(len(modules) == 2 and {r.get('kind') for r in modules} == {0, 1}, 'two native API modules')
    check(all(r.get('loaded_from_system32') is True and isinstance(r.get('was_loaded'), bool) and
              r.get('module', 0) > 0 and r.get('target', 0) > 0 and
              rows.index(start) < rows.index(r) < rows.index(prepared) for r in modules), 'system API module preparation')
    targets = [next(r['target'] for r in modules if r['kind'] == k) for k in (0, 1)]
    check(prepared.get('targets') == targets and len(set(targets)) == 2 and
          prepared.get('queued_api_hooks') == 2 and prepared.get('sdk_initialized') is False and
          isinstance(prepared.get('original_heads_rva'), int) and prepared['original_heads_rva'] > 0, 'original API metadata')
    check(init.get('before_factory') is False and init.get('caller_rva') == 0x23a4e2e and
          init.get('after_native_device_creation') is True and init.get('before_native_queues_and_swapchain') is True and
          init.get('sleep') is False and init.get('markers') is False, 'manual main-device initialization')
    check(device.get('caller_rva') == 0x23a4e2e and device.get('thread') == init.get('thread') and
          isinstance(device.get('thread'), int) and device['thread'] > 0 and device.get('device', 0) > 0 and
          device['device'] == init.get('device'), 'main device identity')
    check(device.get('low_latency_available') is True and device.get('mode') == 0 and
          device.get('rendering_modified') is False and device.get('sleep_calls') == 0 and
          device.get('markers') == 0, 'device capability/off mode')
    check(shutdown.get('passed') is True and shutdown.get('before_native_device_use_resumes') is True, 'SDK shutdown')
    heads = [r for r in rows if r.get('event') == 'hook_restoration']
    check(len(heads) == 2 and {r.get('kind') for r in heads} == {0, 1}, 'two original API heads')
    check(all(r.get('restored') is True and r.get('target', 0) > 0 and
              isinstance(r.get('original_head_sha256'), str) and
              bool(re.fullmatch('[0-9a-f]{64}', r['original_head_sha256'])) for r in heads), 'API head restoration')
    check(all(r['target'] == targets[r['kind']] and rows.index(shutdown) < rows.index(r) < rows.index(swap)
              for r in heads), 'restored API targets/order')
    check(swap.get('native_interface') is True and swap.get('width', 0) > 0 and
          swap.get('height', 0) > 0 and swap.get('format', 0) > 0 and swap.get('buffers', 0) >= 2, 'native swap-chain metadata')
    for key in ('startup_passed', 'hooks_restored', 'sdk_shutdown_passed', 'swapchain_observed'):
        check(finish.get(key) is True, key)
    check(type(finish.get('qualified_factories')) is int and finish['qualified_factories'] >= 0 and finish.get('qualified_devices') == 1 and
          finish.get('pending_calls') == 0 and finish.get('swapchain_proxy_installed') is False and
          finish.get('game_reflex_active') is False, 'transaction completion')
    return dict(schema=1, startup_passed=True, sdk_shutdown_passed=True, original_api_heads_restored=2,
        pid=start.get('pid'), creation=start.get('creation'), probe_sha256=start['dll_sha256'],
        low_latency_available=True, native_swapchain={k: swap[k] for k in ('width', 'height', 'format', 'buffers', 'windowed')},
        frame_markers=0, sleep_calls=0, game_reflex_active=False)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    raw = args.log.read_bytes()
    result = validate([json.loads(row) for row in raw.splitlines()])
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        with args.output.open('x', encoding='utf-8') as out:
            out.write(text)
    print(text, end='')

if __name__ == '__main__':
    main()

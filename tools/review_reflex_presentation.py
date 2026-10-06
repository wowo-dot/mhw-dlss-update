"""Strict review of persistent Reflex Off presentation; no process access."""
from pathlib import Path
import argparse
import hashlib
import json
import re

HOST_SHA = 'c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'

def check(ok, reason):
    if not ok:
        raise ValueError(reason)

def validate(rows):
    def one(event):
        matches = [row for row in rows if row.get('event') == event]
        check(len(matches) == 1, event)
        return matches[0]
    start = one('start')
    check(start.get('schema') == 1 and start.get('version') == '0.2.1' and
          start.get('experiment') == 'reflex-persistent-presentation', 'version')
    check(start.get('host_sha256') == HOST_SHA and start.get('graphics_api') == 'D3D12' and
          start.get('sdk') == '2.12.0', 'runtime identity')
    check(type(start.get('pid')) is int and start['pid'] > 0 and
          type(start.get('creation')) is int and start['creation'] > 0 and
          re.fullmatch('[0-9a-f]{64}', start.get('dll_sha256', '')), 'process/probe identity')
    check(start.get('mode') == 0 and all(start.get(k) is False for k in ('sleep','markers','upscaling')), 'Off only')
    check(not any(row.get('event') in ('failed','worker_failed','runtime_rejection') for row in rows), 'runtime rejection')
    modules = [row for row in rows if row.get('event') == 'api_module']
    check(len(modules) == 2 and {row.get('kind') for row in modules} == {0,1} and
          all(row.get('loaded_from_system32') is True for row in modules), 'system modules')
    prepared = one('prepared'); one('ready')
    targets = [next(row['target'] for row in modules if row['kind'] == i) for i in (0,1)]
    check(prepared.get('targets') == targets and prepared.get('queued_api_hooks') == 2 and
          prepared.get('sdk_initialized') is False and prepared.get('original_heads_rva',0) > 0, 'creation hook metadata')
    init = one('sdk_initialized'); device = one('device_checked'); installed = one('presentation_installed')
    context = one('factory_context')
    check(context.get('source') == 'native-adapter-parent' and context.get('output_owner_matches') is True and
          context.get('factory',0) > 0 and context['factory'] == installed.get('factory') and
          context.get('backend',0) > 0 and isinstance(context.get('global_backend_published'),bool), 'factory context')
    check(init.get('after_native_device_creation') is True and init.get('before_native_queues_and_swapchain') is True and
          init.get('caller_rva') == 0x23a4e2e and init.get('sleep') is False and init.get('markers') is False, 'initialization order')
    check(device.get('device',0) > 0 and device['device'] == init.get('device') and
          device.get('thread',0) > 0 and device['thread'] == init.get('thread') and
          device.get('caller_rva') == 0x23a4e2e and device.get('low_latency_available') is True and
          device.get('mode') == 0 and device.get('rendering_modified') is False and
          device.get('sleep_calls') == 0 and device.get('markers') == 0, 'native device/Off')
    check(installed.get('device_native') is True and installed.get('returned_interfaces_native') is True and
          installed.get('factory',0) > 0 and installed.get('creation_routes') == 4 and installed.get('mode') == 0, 'native host routing')
    swap = one('swapchain_snapshot'); ready = one('persistent_ready')
    check(swap.get('native_interface') is True and swap.get('swapchain',0) > 0 and
          all(swap.get(k,0) > 0 for k in ('width','height','format','buffers')) and
          isinstance(swap.get('windowed'),bool), 'native swap')
    check(ready.get('mode') == 0 and ready.get('sleep_calls') == 0 and ready.get('markers') == 0 and
          ready.get('ceiling_seconds') == 600, 'persistent ready')
    cleanup = one('presentation_cleanup'); shutdown = one('sdk_shutdown'); finish = one('finished')
    check(cleanup.get('restored') is True and cleanup.get('heads_rva',0) > 0 and
          cleanup.get('head_record_bytes') == 48, 'presentation metadata')
    check(shutdown.get('passed') is True and shutdown.get('after_presentation_stop') is True, 'SDK shutdown')
    check(all(finish.get(k) is True for k in ('startup_passed','hooks_restored','sdk_shutdown_passed','swapchain_observed','pins_released')) and
          finish.get('game_reflex_active') is False and finish.get('swapchain_proxy_installed') is False and
          finish.get('qualified_devices') == 1 and finish.get('pending_calls') == 0 and
          finish.get('presentation_pending') == 0 and finish.get('faults') == 0, 'clean completion')
    check(type(finish.get('presents')) is int and finish['presents'] > 0 and
          finish.get('native_presents') == finish['presents'] and finish.get('queues',0) > 0 and
          finish.get('swapchains',0) > 0, 'one native Present per routed frame')
    for event, count in (('hook_restoration',2),('presentation_hook_restoration',12)):
        heads = [row for row in rows if row.get('event') == event]
        check(len(heads) == count and {row.get('kind') for row in heads} == set(range(count)) and
              len({row.get('target') for row in heads}) == count and
              all(row.get('target',0) > 0 and row.get('restored') is True and
                  re.fullmatch('[0-9a-f]{64}',row.get('original_head_sha256','')) for row in heads), event)
        if event == 'hook_restoration':
            check(all(row['target'] == targets[row['kind']] for row in heads), 'creation targets')
        check(all(rows.index(row) < rows.index(cleanup) for row in heads), 'restoration before cleanup')
    order = (start,prepared,init,device,context,installed,swap,ready,cleanup,shutdown,finish)
    check(all(rows.index(a) < rows.index(b) for a,b in zip(order,order[1:])), 'lifecycle order')
    heartbeats = [row for row in rows if row.get('event') == 'heartbeat']
    previous = 0
    for row in heartbeats:
        check(rows.index(ready) < rows.index(row) < rows.index(cleanup) and row.get('faults') == 0 and
              row.get('installed_hooks') == 12 and row.get('presents',-1) >= previous and
              row.get('native_presents') == row.get('presents') and row.get('native_swapchain',0) > 0 and
              row.get('proxy_swapchain',0) > 0 and row['native_swapchain'] != row['proxy_swapchain'], 'heartbeat')
        previous = row['presents']
    check(finish['presents'] >= previous, 'final counter')
    return dict(schema=1,presentation_passed=True,pid=start['pid'],creation=start['creation'],
        probe_sha256=start['dll_sha256'],presents=finish['presents'],native_presents=finish['native_presents'],
        queues=finish['queues'],swapchains=finish['swapchains'],resizes=finish.get('resizes',0),
        faults=0,hooks_restored=14,sdk_shutdown_passed=True,pins_released=True,
        native_device=device['device'],native_factory=installed['factory'],
        sleep_calls=0,markers=0,game_reflex_active=False)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path); parser.add_argument('--output',type=Path)
    args=parser.parse_args(); raw=args.log.read_bytes()
    result=validate([json.loads(line) for line in raw.splitlines()]); result['log_sha256']=hashlib.sha256(raw).hexdigest()
    if args.output:
        with args.output.open('x',encoding='utf-8') as out: json.dump(result,out,indent=2); out.write('\n')
    print(json.dumps(result))

if __name__=='__main__': main()

"""Review owned-pass scheduling and neutral CPU behavior; no GPU/DLAA claim."""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
from arm_aa_boundary import parse, require


def review(raw, dll_sha, deferred=False, gpu=False, pixels=False, projection=False, sdk=False):
    projection = projection or sdk
    pixels = pixels or projection
    gpu = gpu or pixels
    deferred = deferred or gpu
    rows = parse(raw, owned=True, deferred=deferred, gpu=gpu, pixels=pixels, projection=projection, sdk=sdk)
    require(rows[0].get('dll_sha256') == dll_sha.lower(), 'DLL identity')
    require([r['event'] for r in rows] == ['start', 'ready', 'prepared', 'armed'] + ['sample'] * 8 + (['deferred'] if deferred else []) + (['gpu_trace'] if gpu else []) + (['input_readbacks'] if pixels else []) + (['projection'] if projection else []) + (['aa_sdk'] if sdk else []) + ['stopped'], 'complete event sequence')
    require(rows[1]['reserved'] and not rows[1]['hooks_installed'], 'startup')
    prepared = rows[2]
    require(prepared['ok'] and prepared['graphics_api'] == 'D3D12' and prepared['width'] == 2560 and prepared['height'] == 1440 and prepared['device'] > 0, 'discovery')
    require(prepared['swapchain_format'] == 87, 'swap-chain format')
    require(rows[3]['hooks'] == (25 if projection else 6 if gpu else 2 if deferred else 1), 'hook scope')
    final = rows[-1]
    require(final['samples'] == final['visits'] == 8 and final['fault'] == final['pending'] == 0 and
            final['drained'] and final['hooks_disabled'] and final['owned_descriptors_detached'], 'clean closure')
    base = rows[0]['base']
    expected = [(0, base+0x3538400, [0, 0, 0]), (100, base+0x3537a70, [0, 0, 0]),
                (1024, base+0x3536c98, [0, 1, 0]), (134217713, 0, [1, 0, 0])]
    frames, resources, deltas = [], [], []
    descriptors = set()
    for i, sample in enumerate(rows[4:12]):
        require(sample['index'] == i and all(sample[k] for k in ('entered', 'returned', 'complete', 'same_frame')), 'sample completion')
        before, after, owned = sample['before'], sample['after'], sample['owned']
        require(all(owned[k] for k in ('inserted', 'removed', 'returned', 'complete', 'callback_valid', 'neutral', 'in_place')) and
                owned['reason'] == 0 and owned['callbacks'] == 1, 'owned callback completion')
        require(owned['phase'] == 15 and owned['order'] == 2 and owned['thread'] == sample['thread'], 'owned placement/thread')
        require(before['enabled'] == before['collecting'] == 1 and before['taa'] == before['fxaa'] == 0 and
                before['count'] == 4 and len(before['nodes']) == 4, 'AA-Off admission')
        require(before['rectangle'] == [0, 0, 2560, 1440], 'full scene')
        owners = [before['attachments'][0], *before['scratch'], before['attachments'][1]]
        require(len(set(owners)) == 4 and all(x > 0 for x in owners), 'distinct owners')
        native = sorted(before['nodes'], key=lambda n: n['order'])
        require(len({n['descriptor'] for n in native}) == 4, 'unique native descriptors')
        for n, (order, vt, flags), src, dst in zip(native, expected, (owners[0], owners[1], 0, owners[2]), (owners[1], owners[2], owners[2], owners[3])):
            require(n['phase'] == 15 and n['order'] == order and n['vtable'] == vt and n['flags'] == flags and n['input'] == src and n['output'] == dst, 'native graph')
        for q in (owned['callback'], owned['after'], owned['restored'], after):
            for key in ('manager', 'context', 'view', 'renderer', 'frame', 'epoch', 'bank', 'rectangle', 'taa', 'fxaa', 'scratch', 'attachments', 'enabled'):
                require(q[key] == before[key], 'phase identity: ' + key)
            require(q['collecting'] == 0 and q['packets'] >= before['packets'], 'phase state')
        require(owned['descriptor'] > 0 and owned['unit'] > 0 and owned['vtable'] > 0 and owned['descriptor'] not in descriptors, 'owned lifetime identity')
        descriptors.add(owned['descriptor'])
        inserted = dict(descriptor=owned['descriptor'], unit=owned['unit'], vtable=owned['vtable'], phase=15, order=2,
                        input=0, output=owners[1], flags=[0, 1, 0])
        want = sorted(native + [inserted], key=lambda n: n['order'])
        for q in (owned['callback'], owned['after']):
            require(q['count'] == 5 and q['nodes'] == want, 'inserted graph unchanged')
        for q in (owned['restored'], after):
            require(q['count'] == 4 and q['nodes'] == before['nodes'], 'exact native queue restored')
        require(owned['restored'] == after, 'restoration receipt')
        require(owned['packets_before'] == owned['callback']['packets'] and owned['packets_after'] == owned['packets_before'] + int(deferred), 'callback packet count')
        if deferred:
            require(owned.get('published') is True, 'deferred publication')
        require(owned['color_owner'] == owners[1] and owned['callback_count'] == owned['after_count'] == 5 and
                owned['restored_count'] == 4 and owned['callback_frame'] == before['frame'], 'owned summary')
        d = owned['resource']
        require(d['valid'] and d['identity'] > 0 and d['device'] == prepared['device'] and d['dimension'] == 3 and
                d['width'] == 2560 and d['height'] == 1440 and d['format'] == 26 and d['mips'] == d['array'] == d['samples'] == 1, 'color description')
        frames.append(before['frame']); resources.append(d['identity']); deltas.append(after['packets'] - before['packets'])
    require(len(set(frames)) == 8 and frames == sorted(frames), 'distinct ordered frames')
    return dict(schema=1, reviewed_at=datetime.now().astimezone().isoformat(), accepted=True,
                pid=rows[0]['pid'], creation=rows[0]['creation'], log_sha256=hashlib.sha256(raw).hexdigest(),
                frames=frames, native_packet_deltas=deltas, color_resources=resources, owned_callbacks=8,
                exact_queue_restorations=8, native_aa_off=True, callback_adds_no_packets=not deferred,
                hooks_disabled=True, descriptors_detached=True, gpu_pixels_states_lifetime_verified=False,
                live_independent_dlaa_verified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--dll-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'preserve review')
    require(args.log.stat().st_size <= 1048576, 'log size')
    result = review(args.log.read_bytes(), args.dll_sha256)
    with args.output.open('x') as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

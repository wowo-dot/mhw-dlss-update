"""Verify owned AA packet publication/consumption; does not certify GPU inputs."""
import argparse
import json
from pathlib import Path
from arm_aa_boundary import parse, require
from review_aa_owned import review as owned_review


def review(raw, dll_sha, gpu=False, pixels=False, projection=False, sdk=False):
    result = owned_review(raw, dll_sha, deferred=True, gpu=gpu, pixels=pixels, projection=projection, sdk=sdk)
    rows = parse(raw, deferred=True, gpu=gpu, pixels=pixels, projection=projection, sdk=sdk)
    report = rows[12]
    require(report['fault'] == report['pending'] == 0 and report['hook_disabled'] and
            report['private_gpu_commands'] == 0 and report['gpu_states_verified'] is False, 'deferred closure and scope')
    requests = report['requests']
    require(len(requests) == 8, 'request count')
    packets = set()
    for i, (r, sample) in enumerate(zip(requests, rows[4:12])):
        q, owned = sample['before'], sample['owned']
        require(r['index'] == i and r['generation'] == i+1 and r['stage'] == 4 and r['reason'] == 0 and r['calls'] == 1, 'single consumption')
        require(all(r[k] for k in ('enqueued', 'scope_restored', 'list_valid', 'owners_match', 'parameters_same', 'native_returned', 'pins_released')), 'deferred invariants')
        require(r['packet'] > 0 and r['packet'] not in packets and r['entry_at_publish'] > 0, 'unique packet lifetime')
        packets.add(r['packet'])
        require(r['key'] == (15 << 27) | 2 and r['cached_batch'] == 0 and
                owned['packets_before']+128 <= r['queue_limit'] <= r['queue_capacity'] <= 1048576, 'publication contract')
        for key in ('frame', 'epoch', 'bank'):
            require(r['source_'+key] == q[key], 'immutable source '+key)
        # The global renderer may have advanced before deferred consumption.
        require(r['publish_tick'] <= r['consume_tick'] and r['renderer_frame_at_consume'] > 0, 'consumption timing')
        require(all(r[k] > 0 for k in ('thread', 'executor', 'backend_context', 'recording', 'list')), 'backend identity')
        inputs = r['inputs']
        require(len(inputs) == 3 and len({v['resource'] for v in inputs}) == 3, 'distinct inputs')
        for j, v in enumerate(inputs):
            require(v['role'] == j and v['owner'] > 0 and v['backing'] > 0 and v['resource'] > 0 and
                    v['dimension'] == 3 and v['width'] == 2560 and v['height'] == 1440 and
                    v['mips'] == v['array'] == v['samples'] == 1, 'input description')
        require(inputs[0]['owner'] == owned['color_owner'] and inputs[0]['resource'] == owned['resource']['identity'] and
                inputs[0]['format'] == 26, 'source color link')
    result.update(deferred_requests=8, deferred_consumptions=8, native_parameters_unchanged=True,
                  input_identities_held_through_cpu_consumption=True, gpu_states_verified=False,
                  gpu_completion_verified=False, callback_adds_no_packets=False)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--dll-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists() and args.log.stat().st_size <= 1048576, 'artifact bounds')
    result = review(args.log.read_bytes(), args.dll_sha256)
    with args.output.open('x') as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result, indent=2))

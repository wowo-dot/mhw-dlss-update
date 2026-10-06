"""Review bounded CPU jitter samples from the diagnostic trace; never qualify GPU use."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
from review_submission import require, review_projection, unique_object


def floats(bits):
    require(all(type(v) is int and 0 <= v <= 0xFFFFFFFF for v in bits), "float bits")
    values = list(struct.unpack('<'+'f'*len(bits), struct.pack('<'+'I'*len(bits), *bits)))
    require(all(math.isfinite(v) for v in values), "nonfinite gate values")
    return values


def review_rows(rows, state):
    triggered = state['producer'] == 1500
    require((state['schema'],state['producer']) in ((1,1400),(2,1500)) and state['stop'] in (0, 1, 2, 3), "sequence state")
    require(0 < len(rows) <= 32 and len(rows) == state['count'], "sequence count")
    counts, last_ticks, last_frames = [0, 0], [None, None], [None, None]
    result = []
    for index, s in enumerate(rows):
        require(s['schema'] == state['schema'] and s['producer'] == state['producer'] and s['index'] == index and s['phase'] in (0, 1), "sample identity")
        phase, tick = s['phase'], s['tick']
        require(not s['gpu_execution_verified'], "unsupported GPU claim")
        require((index != 0 or (phase == 0 and tick == state['started'])) and
                0 <= tick-state['started'] < 120000 and (not index or tick >= rows[index-1]['tick']), "sequence time")
        require(counts[phase] < 16 and (last_ticks[phase] is None or tick-last_ticks[phase] >= 100), "phase budget or spacing")
        if triggered and phase == 1:
            require(counts[0] == 16 and state['armed'] >= last_ticks[0] and state['armed'] <= tick, "scene trigger timing")
        if triggered and phase == 0:
            require(counts[1] == 0, "initial phase after scene")
        counts[phase] += 1; last_ticks[phase] = tick
        packet = s['frame_packet']; p = packet['producer']; g = s['gates']
        require(packet['schema'] == 2 and not any(packet[k] for k in ('queued', 'consumer_bound', 'evaluate_bound', 'publication_bound', 'temporal_alignment_verified', 'shader_jitter_verified', 'jitter_units_verified')),
                "sequence must not claim a submitted frame")
        duplicate = p['valid'] and last_frames[phase] is not None and p['frame'] <= last_frames[phase]
        require(s['duplicate'] == duplicate, "duplicate frame flag")
        if p['valid'] and not duplicate: last_frames[phase] = p['frame']
        if p['valid']:
            require(p['checkpoint'] == 100 and p['frame'] == p['frame_after'] and p['bank'] in (0, 1) and p['async_mode'] in (0, 1) and
                    p['recording_context'] == p['renderer']+0x710 and p['rect'] == [0, 0, 2560, 1440] and
                    p['current_index'] == p['frame'] % 3 and p['next_index'] == (p['frame']+1) % 3, "source frame anchors")
        projection = review_projection(p, typed=True) if p['projection']['attempted'] else None
        if g['valid']:
            require(g['attempted'] and g['rejection'] == 0 and 0 < g['reads'] <= 40 and g['layer'] and g['wrapper_slot'] and
                    g['vtable_rva'] == 0x351BDA8 and g['vtable'] > g['vtable_rva'] and g['enabled'] in (0, 1) and g['use_dlss'] in (0, 1), "gate anchors")
            blend = floats([g['blend_bits']])[0]; floats(g['reset_vector_bits'])
            require(len(g['reset_vector_bits']) == 2, "reset vector")
            predicate = not (g['view_flags'] & 0x840000) and blend != 0 and g['layer_type'] == 8 and g['layer_flags'] & 0x12 == 0x12 and g['enabled'] != 0
            require(g['predicate'] == predicate, "native jitter predicate")
        else:
            require(not g['predicate'] and (not g['attempted'] or 1 <= g['rejection'] <= 5), "invalid gate observation")
        valid = p['valid'] and p['projection']['valid'] and g['valid'] and not duplicate and (triggered or bool(g['wrapper']) == (phase == 0))
        require(s['valid'] == valid, "sample validity")
        result.append(dict(index=index, phase=phase, frame=p['frame'], epoch=p['epoch'], valid=valid, duplicate=duplicate,
                           gates=g, projection=projection))
    expected_counts = [state['initial'],state['scene']] if triggered else [state['present'],state['absent']]
    require(counts == expected_counts and (state['stop'] != 1 or counts == [16, 16]), "terminal counts")
    if triggered and state['armed']:
        require(counts[0] == 16 and last_ticks[0] <= state['armed'] < state['started']+120000, "scene arm state")
    phases = []
    for phase in (0, 1):
        selected = [r for r in result if r['phase'] == phase and r['valid']]
        zero = sum(all(v == 0 for v in r['projection']['stored_camera_jitter']) for r in selected)
        phases.append(dict(phase=phase, samples=counts[phase], valid=len(selected), zero_jitter=zero,
                           wrapper_present=sum(bool(r['gates']['wrapper']) for r in selected),
                           wrapper_absent=sum(not r['gates']['wrapper'] for r in selected),
                           nonzero_jitter=len(selected)-zero, predicate_true=sum(r['gates']['predicate'] for r in selected),
                           all_matrix_relations=bool(selected) and all(r['projection']['matrix_relation'] for r in selected)))
    return dict(schema=state['schema'], producer=state['producer'], stopped=state['stop'] != 0, stop_reason=state['stop'],
                phase_meaning='initial-and-triggered-scene' if triggered else 'wrapper-present-and-absent',
                scene_armed_tick=state.get('armed',0),
                phases=phases, samples=result, gpu_execution_verified=False, raster_jitter_verified=False)


def parse_trace(text):
    version = 1500 if 'trace.start version=0.15.0 ' in text else 1400 if 'trace.start version=0.14.0 ' in text else 0
    require(version, "trace version")
    require('bridge.jitter-sample-error' not in text, "sample serialization failed")
    rows, state = [], None
    for line in text.splitlines():
        if 'bridge.jitter-sample ' in line:
            rows.append(json.loads(line.split('bridge.jitter-sample ', 1)[1], object_pairs_hook=unique_object))
        elif 'bridge.jitter-sequence-state ' in line:
            state = json.loads(line.split('bridge.jitter-sequence-state ', 1)[1], object_pairs_hook=unique_object)
    require(state is not None, "sequence state missing")
    require(state['producer'] == version, "trace producer mismatch")
    return review_rows(rows, state)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    raw = args.log.read_bytes(); result = parse_trace(raw.decode('utf-8-sig'))
    result['trace_sha256'] = hashlib.sha256(raw).hexdigest().upper()
    result['owned_fixture'] = 'owned-fixture=true' in raw.decode('utf-8-sig')
    with args.output.open('x', encoding='utf-8') as out: json.dump(result, out, indent=2)
    print(json.dumps({k: result[k] for k in ('stopped', 'stop_reason', 'phases', 'gpu_execution_verified')}, indent=2))


if __name__ == '__main__':
    main()

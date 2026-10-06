"""Replay saved v1.18.1 translation-reset matrix pairs through the native fix."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
from continuous_history import camera_center, example, require


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log', type=Path)
    ap.add_argument('--status', type=Path, action='append', default=[])
    ap.add_argument('--executable', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    require(not a.output.exists(), 'preserve replay')
    raw = a.log.read_bytes()
    rows = [json.loads(line) for line in raw.decode('utf-8').splitlines()]
    require(rows[0]['version'] == '1.18.1', 'source version')
    statuses = [r for r in rows if r['event'] == 'continuous']
    statuses += [json.loads(p.read_bytes()) for p in a.status]
    pairs = {}
    for s in statuses:
        h = s['history']
        require(h['schema'] == 1 and h['policy'] == 'v1.18.0-unchanged', 'source policy')
        examples = [x['example'] for x in h['first']] + ([h['latest']] if h['latest'] else [])
        for e in examples:
            if e['reasons'] != 1024:
                continue
            example(e)
            pairs[(e['previous']['frame'], e['next']['frame'])] = e
    require(pairs, 'captured pairs')
    selected = [v for _, v in sorted(pairs.items())]
    payload = ''.join(' '.join(str(v) for v in e['previous']['view_bits'] + e['next']['view_bits']) + '\n' for e in selected)
    rows = subprocess.check_output([str(a.executable.resolve()), '--replay'], input=payload, text=True).splitlines()
    require(len(rows) == len(selected), 'native replay rows')
    output = []
    for e, line in zip(selected, rows):
        valid, reset, reasons, distance, jumps = line.split()
        require(valid == '1' and reset == '0' and reasons == '0', 'native history retention')
        require(int(jumps) == e['view_jump_mask'], 'same legacy diagnostic mask')
        centers = [camera_center([struct.unpack('<f', struct.pack('<I', v))[0] for v in e[k]['view_bits']]) for k in ('previous', 'next')]
        expected = math.dist(*centers)
        require(math.isclose(float(distance), expected, rel_tol=1e-9, abs_tol=1e-7), 'independent camera solve')
        output.append(dict(frame=e['next']['frame'], old_reasons=e['reasons'], new_reasons=0, camera_distance=float(distance), retained=True))
    result = dict(accepted=True, samples=len(output), source_log_sha256=hashlib.sha256(raw).hexdigest(),
                  status_sources=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in a.status],
                  executable_sha256=hashlib.sha256(a.executable.read_bytes()).hexdigest(), examples=output,
                  limitation='Offline typed-matrix replay; not live transition or image-quality acceptance.')
    with a.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

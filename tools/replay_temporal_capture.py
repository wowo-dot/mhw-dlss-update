"""Verify a sealed v1.9.0 capture, replay its typed inputs, compare every GPU value."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path
import numpy as np


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--capture-dir', type=Path, required=True)
    ap.add_argument('--pid', type=int, required=True)
    ap.add_argument('--archive-sha256', required=True)
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    root = args.capture_dir.resolve()
    archive = root / f'archive-{args.pid}.json'
    require(sha(archive) == args.archive_sha256.lower(), 'archive manifest identity')
    manifest = json.loads(archive.read_text())
    for item in manifest['files']:
        path = (root / item['path']).resolve()
        require(path.is_relative_to(root), 'archive path outside capture directory')
        require(path.stat().st_size == item['bytes'] and sha(path) == item['sha256'], f"archive integrity: {item['path']}")
    logs = list(root.glob(f'inputs-{args.pid}-*.jsonl'))
    require(len(logs) == 1 and logs[0].stat().st_size < 1048576, 'one bounded capture log required')
    raw = logs[0].read_bytes()
    header = json.loads(raw.splitlines()[0])
    require(header['version'] == '1.9.0' and header['pid'] == args.pid, 'supported recorder version / pid')
    sys.path.insert(0, str(root / 'source-1.9.0/tools'))
    from review_temporal_inputs import parse, review
    rows = parse(raw)
    checked = dict(review(rows, root / header['shader_file']), log_sha256=sha(logs[0]))
    require(checked == json.loads((root / f'review-{args.pid}.json').read_text()), 'frozen review reproduction')
    require(checked['accepted'] and checked['depth_color_motion_triplets_verified'], 'accepted triplets required')
    capture = next(row for row in rows if row['event'] == 'capture')
    low, high = capture['adapter_luid_low'], capture['adapter_luid_high']
    args.output.mkdir(parents=True, exist_ok=False)
    exe = args.output / 'mhw_temporal_input_replay.exe'
    shutil.copyfile(args.exe, exe)
    require(sha(exe) == sha(args.exe), 'preserved executable identity')
    results = []
    for index, frame in enumerate(checked['frames']):
        inputs = [checked[k][index] for k in ('color_readbacks', 'scene_depth_readbacks', 'motion_readbacks')]
        width, height = inputs[0]['width'], inputs[0]['height']
        require(all((p['frame'], p['width'], p['height']) == (frame, width, height) for p in inputs), 'matched dimensions/frame')
        prefix = args.output / f'frame-{frame}'
        command = [str(exe.resolve()), '--adapter-luid', str(low), str(high & 0xffffffff), '--width', str(width), '--height', str(height),
                   '--color', str(root / inputs[0]['file']), '--depth', str(root / inputs[1]['file']),
                   '--motion', str(root / inputs[2]['file']), '--output', str(prefix.resolve())]
        ran = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (args.output / f'frame-{frame}.stdout.txt').write_text(ran.stdout)
        (args.output / f'frame-{frame}.stderr.txt').write_text(ran.stderr)
        require(ran.returncode == 0, f'replay rejected frame {frame}: {ran.stderr}')
        report = json.loads(Path(str(prefix) + '.json').read_text())
        require(report['adapter_luid_low'] == low and report['adapter_luid_high'] == high, 'same adapter as capture')
        require(report['debug_layer'] and report['gpu_validation'] and report['debug_errors'] == report['debug_warnings'] == 0,
                'clean D3D12/GPU validation required for this verification')
        require(report['completed'] and report['allocator_retired'] and report['all_pixels_match_cpu'], 'replay completion / C++ reference')
        require(report['input_srv_formats'] == [26, 41, 42] and report['output_uav_formats'] == [2, 41, 16], 'explicit view formats')
        require(report['sdk_calls'] == 0 and not report['game_process_access'] and not report['dlaa_ready'], 'offline scope')

        words = np.fromfile(root / inputs[0]['file'], dtype='<u4')
        color = []
        for offset, bits in ((0, 6), (11, 6), (22, 5)):
            values = words >> offset
            exponent = ((values >> bits) & 31).astype(np.int32)
            mantissa = (values & ((1 << bits) - 1)).astype(np.float32)
            require(np.all(exponent != 31), 'finite color')
            channel = np.where(exponent == 0, np.ldexp(mantissa, 1 - 15 - bits),
                               np.ldexp(1 + mantissa / (1 << bits), exponent - 15))
            color.append(channel.astype(np.float32))
        color.append(np.ones(words.size, dtype=np.float32))
        expected_color = np.stack(color, axis=-1)
        expected_depth = np.fromfile(root / inputs[1]['file'], dtype='<f4')
        words = np.fromfile(root / inputs[2]['file'], dtype='<u4')
        dx = (words >> 16).astype('<u2').view('<f2').astype(np.float32)
        dy = (words & 0xfffe).astype('<u2').view('<f2').astype(np.float32)
        expected_motion = np.stack((-dx * np.float32(width * .5), dy * np.float32(height * .5)), axis=-1)
        outputs = []
        for name, expected in (('color', expected_color), ('depth', expected_depth), ('motion', expected_motion)):
            path = Path(str(prefix) + f'.{name}.f32')
            require(path.stat().st_size == expected.size * 4, f'{name} output extent')
            actual = np.fromfile(path, dtype='<f4').reshape(expected.shape)
            require(np.isfinite(actual).all() and np.array_equal(actual, expected), f'{name} full-image independent comparison')
            outputs.append(dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path), compared_values=int(expected.size),
                                max_abs_error=float(np.max(np.abs(actual - expected)))))
        require(Path(str(prefix) + '.depth.f32').read_bytes() == (root / inputs[1]['file']).read_bytes(), 'depth bit preservation')
        result = dict(frame=frame, inputs=inputs, outputs=outputs, report_file=prefix.name + '.json',
                      report_sha256=sha(Path(str(prefix) + '.json')), gpu_validation_clean=True,
                      depth_bit_exact=True, color_numeric_exact=True, history_motion_numeric_exact=True)
        results.append(result)
        print(json.dumps(dict(frame=frame, passed=True, pixels=width * height, all_values_match=True)), flush=True)
    result = dict(schema=1,source_archive_sha256=sha(archive),source_archive_files=len(manifest['files']),
                  source_log_sha256=sha(logs[0]),host_sha256=header['host_sha256'],recorder_dll_sha256=header['dll_sha256'],
                  replay_exe_sha256=sha(exe),adapter_luid=[low,high],numpy_version=np.__version__,frames=results,
                  owned_typed_gpu_loads_verified=True,owned_motion_conversion_verified=True,
                  engine_descriptor_contents_verified=False,live_owned_copy_integration_verified=False,
                  sdk_evaluation_verified=False,dlaa_ready=False,
                  scope='Sealed captured pixels uploaded into standalone owned D3D12 textures and explicit views. Every GPU output compared with C++ and independent NumPy references; no live game access or SDK calls.')
    with (args.output / 'verification.json').open('x') as out:
        json.dump(result,out,indent=2,allow_nan=False)


if __name__ == '__main__':
    main()

"""Verify one same-list DLAA replacement; this does not certify continuous DLAA."""
import hashlib
import json
import math
from pathlib import Path
from projection_contract import floats
from temporal_shader import require
from temporal_sdk import review_startup_link


def review_contract(header, capture):
    require(header['version'] == '1.17.0' and header['inline_receipt'] == 4 and
            header['presentation_writes'] == header['sdk_evaluation_limit'] == 1 and
            header['inline_same_list'] is True, 'inline version/budget')
    inline = capture['inline_dlaa']; p = inline['pass']; i = inline['receipt']
    require(inline['schema'] == 1 and i == 4 and inline['failure'] == '' and
            all(inline[k] is True for k in ('attempted', 'bindings_equal', 'saved')), 'inline completion')
    require(p['schema'] == 1 and p['failure'] == '' and all(p[k] is True for k in
            ('creation_completed', 'attempted', 'issued', 'restored', 'copied', 'collected', 'released', 'shutdown_deferred')), 'inline SDK closure')
    require(all(type(p[k]) is int and p[k] == 1 for k in ('create_result', 'evaluate_result', 'release_result', 'destroy_result')), 'inline SDK API results')
    require(type(p['allocation_bytes']) is int and 0 < p['allocation_bytes'] <= header['sdk_image_byte_limit'] == 419430400, 'inline allocation budget')
    require(p['device'] == capture['device'] and p['width'] == capture['width'] and p['height'] == capture['height'], 'inline device/extent')
    require([p[k] for k in ('create_flags', 'mode', 'preset', 'input_formats', 'converted_formats', 'mv_scale', 'pre_exposure')] ==
            [75, 5, 11, [26, 41, 42], [2, 41, 16], [1, 1], 1] and p['mv_jittered'] is False and p['reset'] is True, 'inline SDK contract')
    require(type(p['frame_ms']) in (int, float) and math.isfinite(p['frame_ms']) and abs(p['frame_ms']-16.66666603088379) < 1e-6, 'inline diagnostic time')
    r = capture['records'][i]; s = capture['submission']['samples'][i]; b = capture['native_bindings']['records'][i]
    require(p['frame'] == r['frame'] == s['frame'] == b['frame'] and p['epoch'] == r['epoch'], 'inline frame association')
    require(p['generation'] == s['reset'] == b['generation'] and p['command'] == s['command'] == b['command'], 'inline command generation')
    require(p['completion_fence'] == s['fence'] and p['completion_value'] == s['completed_value'] == 1, 'inline exact completion fence')
    require(p['native_output'] == r['inputs'][4]['resource'] and p['raw'] == capture['owned_gpu_inputs']['sets'][i]['inputs'], 'inline resource association')
    x, y = floats(capture['projection_stimulus']['records'][i]['jitter_bits'], 2)
    require(len(p['jitter']) == 2 and all(type(v) in (int, float) and math.isfinite(v) for v in p['jitter']) and
            all(abs(a-b) < 1e-8 for a, b in zip(p['jitter'], (x/2, -y/2))), 'inline projection jitter')
    require(len(capture['submission']['samples']) == 9, 'inline capture length')
    for index, sample in enumerate(capture['submission']['samples']):
        if index != i:
            require(sample['inline_begin'] == sample['inline_end'] == 0 and sample['inline_ok'] is False, 'inline one-frame limit')
            continue
        require(sample['inline_ok'] is True, 'inline recording result')
        order = [sample[k] for k in ('depth_end', 'dispatch', 'copy_begin', 'copy_end', 'color_begin', 'color_end',
                                    'dispatch_forward', 'inline_begin', 'inline_end', 'dispatch_exit', 'close', 'execute')]
        require(all(type(n) is int and n > 0 for n in order) and all(a < b for a, b in zip(order, order[1:])), 'inline native ordering')
        require(sample['completed'] is True and sample['completed_value'] == 1 and sample['retire_exit'] > sample['execute_exit'], 'inline completion/retirement')
    state = capture['native_compute']['records'][i]['states'][4]
    require(state == dict(known=True, conflict=False, value=8), 'inline native UAV state')
    sdk = capture['owned_gpu_inputs']['sdk']
    require(sdk['attempted'] is False and sdk['saved'] is False, 'inline no deferred SDK evaluation')
    return i


def pack_reference(rgb):
    import numpy as np
    packed = np.zeros(rgb.shape[0], dtype='<u4')
    for channel, bits, shift in ((0, 6, 0), (1, 6, 11), (2, 5, 22)):
        maximum = math.ldexp(2-math.ldexp(1, -bits), 15)
        value = np.clip(rgb[:, channel].astype(np.float64), 0, maximum)
        fraction, exponent = np.frexp(value)
        normal = ((exponent.astype(np.int64)+14) << bits) + np.rint(np.ldexp(fraction, bits+1)).astype(np.int64) - (1 << bits)
        subnormal = np.rint(np.ldexp(value, 14+bits)).astype(np.int64)
        packed |= np.where(value < math.ldexp(1, -14), subnormal, normal).astype('<u4') << shift
    return packed


def review_pixels(header, capture, folder):
    import numpy as np
    i = review_contract(header, capture); folder = Path(folder)
    files = capture['inline_dlaa']['files']; pixels = capture['width'] * capture['height']
    require(len(files) == 5, 'inline artifact count')
    arrays = []
    for index, (suffix, size) in enumerate(zip(('color.f32', 'depth.f32', 'motion.f32', 'dlaa.rgba32f', 'native.u32'), (16, 4, 8, 16, 4))):
        a = files[index]; require(a['file'] == header['motion_prefix']+'.inline-'+suffix, 'inline artifact name')
        path = folder/a['file']; require(path.stat().st_size == a['bytes'] == pixels*size, 'inline artifact size')
        raw = path.read_bytes(); require(hashlib.sha256(raw).hexdigest() == a['sha256'], 'inline artifact hash')
        if index < 3:
            reference = capture['owned_gpu_inputs']['sets'][i]['files'][index]
            require(raw == (folder/reference['file']).read_bytes(), 'inline independent input conversion')
        arrays.append(np.frombuffer(raw, dtype='<u4' if index == 4 else '<f4'))
    rgb = arrays[3].reshape(-1, 4)[:, :3]
    require(np.isfinite(rgb).all() and not np.any(rgb == 65504), 'inline complete finite SDK image')
    original = arrays[0].reshape(-1, 4)[:, :3]
    changed = int(np.count_nonzero(rgb != original))
    require(changed > pixels and float(rgb.std()) > 0, 'inline SDK changed/nonconstant image')
    require(np.array_equal(arrays[4], pack_reference(rgb)), 'inline packed SDK-to-native image')
    return dict(inline_dlaa_verified=True, presentation_changed=True, presentation_frames=1,
                native_binding_restore_verified=True, inline_frame=capture['records'][i]['frame'],
                inline_packed_pixels_checked=pixels, sdk_rgb_values_checked=int(rgb.size),
                inline_changed_rgb_values=changed, sdk_history_retained_verified=False,
                sdk_temporal_quality_verified=False, dlaa_ready=False)


def review_inline(header, capture, folder):
    require(folder is not None, 'inline artifact directory')
    startup = Path(folder)/f"startup-{header['pid']}-{header['creation']}.jsonl"
    raw = startup.read_bytes(); require(0 < len(raw) <= 65536, 'inline startup log size')
    review_startup_link(header, capture, [json.loads(line) for line in raw.decode('utf8').splitlines()])
    result = review_pixels(header, capture, folder)
    result.update(startup_ownership_verified=True, startup_log_sha256=hashlib.sha256(raw).hexdigest())
    return result

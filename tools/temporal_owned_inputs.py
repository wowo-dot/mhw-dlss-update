"""Verify private GPU copies and typed conversion against the raw native pixels."""
from temporal_limits import frame_count
from temporal_scaled_geometry import active_rectangle
import hashlib
from pathlib import Path
from temporal_shader import require


def review_owned(header, capture, folder):
    n=frame_count(header)
    import numpy as np
    owned = capture['owned_gpu_inputs']
    require(all(owned.get(k) is True for k in
                ('enabled', 'prepared', 'producer_qualified', 'attempted', 'saved', 'native_state_restored')),
            'owned capture completion')
    require(all(owned.get(k) is False for k in
                ('failure_present', 'native_bindings_changed', 'device_debug_settings_changed')), 'owned isolation')
    w, h = capture['width'], capture['height']
    require((owned['width'], owned['height']) == (w, h), 'owned dimensions')
    require(folder is not None and len(owned['sets']) == n, 'owned artifact directory/count')
    folder = Path(folder)
    raw_groups = [capture[k]['copies'] for k in ('color_readback', 'depth_readback', 'motion_readback')]
    native = {v[k] for group in raw_groups for v in group for k in ('resource', 'readback_buffer', 'command', 'allocator')}
    native.update(s[k] for s in capture['submission']['samples'] for k in ('queue', 'fence'))
    private = set()
    compared = 0
    reports = []
    for i, s in enumerate(owned['sets']):
        require(s['receipt'] == i and all(s[k] is True for k in ('completed', 'allocator_retired', 'saved')),
                'owned conversion completion/retirement')
        require(s['fence_value'] == 1, 'owned private completion value')
        require([s['adapter_luid_low'], s['adapter_luid_high']] ==
                [capture['adapter_luid_low'], capture['adapter_luid_high']], 'owned adapter identity')
        require(0 < s['allocation_bytes'] <= header['owned_set_byte_limit'], 'owned allocation budget')
        require(s['input_formats'] == [26, 41, 42] and s['output_formats'] == [2, 41, 16] and
                s['srv_mapping'] == 5768, 'owned typed view contract')
        require(s['command_recipe'] == dict(execute=1,signal=1,dispatch=1,copies=3,barriers=6), 'owned command recipe')
        require(len(s['objects']) == 5 and len(s['outputs']) == len(s['readbacks']) == 3, 'owned private objects')
        for address in s['objects'] + s['outputs'] + s['readbacks']:
            require(type(address) is int and 0 < address < 2**64 and address not in native | private,
                    'owned private object identity')
            private.add(address)
        require(len(s['inputs']) == len(s['files']) == 3, 'owned plane count')
        inputs = [group[i] for group in raw_groups]
        for role, (p, address, fmt) in enumerate(zip(inputs, s['inputs'], s['input_formats'])):
            require(type(address) is int and 0 < address < 2**64 and address not in native | private,
                    'owned unique private resource')
            private.add(address)
            require(p['owned_copy'] == dict(resource=address, copied=True, state_before=1024, state_after=1024,
                                           format=fmt, flags=0, subresource=0), 'owned native copy receipt')
            # The existing reviewers prove each raw receipt's exact source,
            # transition, shared recording interval, generation, and completion.
            require(all(p[k] == inputs[0][k] for k in ('receipt', 'frame', 'packet', 'command', 'allocator', 'reset')),
                    'owned same producer frame/generation')
        rect=active_rectangle(header,capture['records'][i],w,h)
        active=np.zeros((h,w),dtype=bool);active[rect[1]:rect[3],rect[0]:rect[2]]=True;active=active.ravel()
        words = np.fromfile(folder / inputs[0]['file'], dtype='<u4')[active]
        channels = []
        for offset, bits in ((0, 6), (11, 6), (22, 5)):
            v = words >> offset
            exponent = ((v >> bits) & 31).astype(np.int32)
            mantissa = (v & ((1 << bits)-1)).astype(np.float32)
            require(np.all(exponent != 31), 'owned finite source color')
            channels.append(np.where(exponent == 0, np.ldexp(mantissa, 1-15-bits),
                                     np.ldexp(1+mantissa/(1 << bits), exponent-15)).astype(np.float32))
        channels.append(np.ones(words.size, dtype=np.float32))
        color = np.stack(channels, axis=-1)
        depth = np.fromfile(folder / inputs[1]['file'], dtype='<f4')[active]
        require(np.isfinite(depth).all() and np.all((depth >= 0) & (depth <= 1)), 'owned finite normalized depth')
        words = np.fromfile(folder / inputs[2]['file'], dtype='<u4')[active]
        x = (words >> 16).astype('<u2').view('<f2').astype(np.float32)
        y = (words & 0xfffe).astype('<u2').view('<f2').astype(np.float32)
        motion = np.stack((-x*np.float32(w*.5), y*np.float32(h*.5)), axis=-1)
        outputs = []
        for role, expected, a in zip(('color', 'depth', 'motion'), (color, depth, motion), s['files']):
            name = f"{header['motion_prefix']}.owned-{i}.{role}.f32"
            require(a['file'] == name and Path(name).name == name, 'owned artifact name')
            path = folder/name
            require(path.stat().st_size == a['bytes'] == w*h*(4 if role=='color' else 2 if role=='motion' else 1)*4, 'owned output size')
            require(hashlib.sha256(path.read_bytes()).hexdigest() == a['sha256'], 'owned output hash')
            actual = np.fromfile(path, dtype='<f4').reshape((w*h,*expected.shape[1:]))[active]
            require(np.isfinite(actual).all() and np.isfinite(expected).all() and np.array_equal(actual, expected),
                    f"owned {role} {'active' if header['version'] in ('1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 'full'}-image comparison")
            if role == 'depth':
                require(np.array_equal(actual.view('<u4'),depth.view('<u4')), 'owned depth bits')
            compared += int(expected.size)
            outputs.append(dict(**a, compared_values=int(expected.size), max_abs_error=0.0))
        reports.append(dict(frame=inputs[0]['frame'], receipt=i, comparison_rectangle=rect, outputs=outputs))
    require(sum(a['bytes'] for s in owned['sets'] for a in s['files']) <= header['owned_readback_limit'],
            'owned output total')
    return dict(live_owned_copy_integration_verified=True, owned_typed_gpu_loads_verified=True,
                owned_motion_conversion_verified=True, owned_compared_float_values=compared,
                owned_gpu_frames=reports, engine_descriptor_contents_verified=False,
                sdk_evaluation_verified=False, dlaa_ready=False)

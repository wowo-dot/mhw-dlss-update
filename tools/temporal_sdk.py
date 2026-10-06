"""Verify the bounded borrowed-device SDK path; never certify temporal quality."""
import hashlib
import json
from pathlib import Path
from temporal_shader import require
from temporal_limits import frame_count
from projection_contract import floats
import math

RUNTIME = 'be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e'

def review_startup_link(header,capture,startup):
    from review_ngx_startup import review as review_startup
    result=review_startup(startup)
    require(result['accepted'] and startup[0]['version']==header['version'] and header['version'] in ('1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'),'startup capability review')
    require(all(startup[0][k]==header[k] for k in ('pid','creation','base','host_sha256','dll_sha256')),'startup process/DLL binding')
    entry=next(r for r in startup if r['event']=='native_enter')
    require(entry['device']==capture['device'] and entry['tick']<=capture['started_tick'],'startup device/timing binding')
    require(result['adapter_luid']==[capture['adapter_luid_high'],capture['adapter_luid_low']] and
            all(result[k]==capture[k] for k in ('width','height','swapchain_format')),'startup graphics binding')

def review_sdk_events(rows, capture):
    owned = capture['owned_gpu_inputs'];n=9 if rows[0]['version']==5 else 3
    require(len(owned['sets'])==n,'SDK owned set count')
    count=rows[0].get('frame_count',3 if n==3 else None)
    require(type(count) is int and count==n,'SDK frame count')
    history=rows[0]['version'] in (4,5)
    if history:
        require(len(rows)==13+5*n,'SDK temporal event count')
        temporal=[r for r in rows if r['event']=='sdk_temporal']
        require(len(temporal)==n,'SDK temporal receipt count')
        projection=capture['projection_stimulus']['records']
        require(len(projection)==len(capture['records'])==n,'SDK projection count')
        for i,r in enumerate(temporal):
            require(rows[n+10+4*i] is r,'SDK temporal event position')
            p=projection[i];t=capture['records'][i]
            require(r['receipt']==i and all(type(r[k]) is int and 0<=r[k]<2**64 and r[k]==p[k]==t[k] for k in ('frame','epoch')),'SDK temporal frame binding')
            if i:require(all(r[k]==temporal[i-1][k]+1 for k in ('frame','epoch')),'SDK temporal adjacency')
            x,y=floats(p['jitter_bits'],2)
            require(len(r['jitter'])==2 and all(type(v) in (int,float) and math.isfinite(v) and abs(v)<=.5 for v in r['jitter']) and
                    all(abs(a-b)<=1e-8 for a,b in zip(r['jitter'],(x/2,-y/2))),'SDK projection jitter mapping')
            require(r['reset'] is (i==0) and r['mv_jittered'] is False,'SDK retained history policy')
            require(r['time_policy']=='fixed-diagnostic' and type(r['frame_ms']) in (int,float) and
                    math.isfinite(r['frame_ms']) and abs(r['frame_ms']-16.66666603088379)<1e-6,'SDK diagnostic time policy')
        rows=[r for r in rows if r['event']!='sdk_temporal']
    require(len(rows) == 13+4*n, 'SDK event count')
    expected = ['sdk_start'] + ['sdk_input']*n + ['sdk_objects'] + ['api']*5 + [
        'sdk_provider', 'api', 'completed'] + ['api', 'completed', 'sdk_output']*n + ['api', 'api', 'sdk_success']
    reuse=rows[0]['version'] in (3,4,5)
    if reuse:expected[n+2]='sdk_reuse'
    require([r['event'] for r in rows] == expected, 'SDK event order')
    start = rows[0]
    require(start['version'] in (1, 2, 3, 4, 5) and start['borrowed_device'] is True and
            start['device'] == capture['device'], 'SDK borrowed device')
    for key in ('width', 'height', 'adapter_luid_low', 'adapter_luid_high'):
        require(start[key] == capture[key], 'SDK extent/adapter')
    require(start['api'] == 21 and start['runtime_sha256'] == RUNTIME and
            start['project_id'] == '956cdd19-7d3c-4660-b664-91c20d2bc9bd', 'SDK identity')
    require(start['formats'] == [2,41,16] and start['output_format'] == 2 and
            start['create_flags'] == 75 and start['mode'] == 5 and start['preset'] == 11, 'SDK create contract')
    require(start['reset_every_frame'] is (not history) and (start['jitter'] is None if history else start['jitter']==[0,0]) and
            start['mv_scale'] == [1,1] and start['pre_exposure'] == 1, 'SDK reset control')
    require(all(start[k] is False for k in ('device_debug_settings_changed', 'native_bindings_changed', 'presentation_changed')), 'SDK isolation')
    require(type(start['legacy_present']) is bool and
            (not start['legacy_present'] or len(start['legacy_sha256']) == 64), 'SDK legacy identity')
    for i, row in enumerate(rows[1:n+1]):
        s = owned['sets'][i]
        require(row['receipt'] == i and row['inputs'] == s['outputs'] and row['input_state'] == 2048,
                'SDK exact owned inputs')
        require(row['producer_fence'] == s['objects'][3] and row['producer_value'] == s['fence_value'] == 1 and
                row['producer_retired'] is True and s['completed'] is True and s['allocator_retired'] is True, 'SDK producer completion')
    objects = rows[n+1]['objects']
    private = {a for s in owned['sets'] for k in ('objects', 'inputs', 'outputs', 'readbacks') for a in s[k]}
    for group in ('color_readback', 'depth_readback', 'motion_readback'):
        private.update(r[k] for r in capture.get(group, {}).get('copies', []) for k in ('resource', 'command', 'allocator', 'readback_buffer'))
    require(len(objects) == len(set(objects)) == 8 and all(type(a) is int and 0 < a < 2**64 and a not in private for a in objects), 'SDK private objects')
    require(0 < rows[n+1]['allocation_bytes'] <= 150994944, 'SDK image budget')
    apis = [r for r in rows if r['event'] == 'api']
    require([r['name'] for r in apis] == ([] if reuse else ['Init_with_ProjectID'])+['GetCapabilityParameters', 'Available', 'NeedsUpdatedDriver',
            'FeatureInitResult', 'CreateFeature'] + ['EvaluateFeature']*n + ['ReleaseFeature', 'DestroyParameters'], 'SDK API order')
    require(all(r['result'] == 1 for r in apis), 'SDK API failure')
    if start['version'] >= 2:
        for row, value in zip(apis[1:4] if reuse else apis[2:5], (1, 0, 1)):
            require(type(row.get('value')) is int and type(row.get('value_u32')) is int and
                    row['value'] == row['value_u32'] == value, 'SDK capability value')
    provider = rows[n+7]
    if reuse:
        reused=rows[n+2]
        require(reused['device']==capture['device'] and reused['module']==provider['module'] and
                reused['ownership_verified'] is True and type(reused['initialization_calls']) is int and
                reused['initialization_calls']==0,'SDK startup ownership')
    require(provider['verified'] is True and provider['distinct_from_legacy'] is True and provider['module'] > 0, 'SDK module identity')
    completed = [r for r in rows if r['event'] == 'completed']
    require([r['phase'] for r in completed] == ['create']+['evaluate']*n and
            [r['fence_value'] for r in completed] == list(range(1,n+2)) and all(r['recording_retired'] is True for r in completed), 'SDK completion/retirement')
    for i, r in enumerate(r for r in rows if r['event'] == 'sdk_output'):
        require(r['receipt'] == i and r['bytes'] == capture['width']*capture['height']*16 and
                all(r[k] is True for k in ('inputs_restored', 'rgb_finite', 'rgb_written')), 'SDK output completion')
    end = rows[-1]
    require(end['evaluations'] == n and all(end[k] is True for k in ('feature_released', 'parameters_destroyed',
            'shutdown_deferred', 'context_retained', 'legacy_unchanged')) and end['temporal_quality_verified'] is False, 'SDK cleanup')


def review_live_sdk(header, capture, folder):
    import numpy as np
    n=frame_count(header)
    require(folder is not None, 'SDK artifact folder')
    folder = Path(folder)
    sdk = capture['owned_gpu_inputs']['sdk']
    require(sdk['attempted'] is True and sdk['saved'] is True, 'SDK completion')
    event = sdk['events']
    require(event['file'] == header['motion_prefix']+'.sdk.jsonl', 'SDK event filename')
    path = folder/event['file']
    require(path.stat().st_size == event['bytes'] and 0 < event['bytes'] <= 131072, 'SDK log bounds')
    require(hashlib.sha256(path.read_bytes()).hexdigest() == event['sha256'], 'SDK log hash')
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    review_sdk_events(rows, capture)
    startup_result={}
    if header['version'] in ('1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(rows[0]['version']==(5 if header['version'] in ('1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 4 if header['version']=='1.14.0' else 3),'SDK startup reuse required')
        startup_path=folder/f"startup-{header['pid']}-{header['creation']}.jsonl"
        raw=startup_path.read_bytes();require(0<len(raw)<=65536,'startup log bounds')
        startup=[json.loads(line) for line in raw.decode('utf-8').splitlines()]
        review_startup_link(header,capture,startup)
        startup_result=dict(startup_ownership_verified=True,startup_log_sha256=hashlib.sha256(raw).hexdigest())
    else:require(rows[0]['version'] in (1,2),'historical SDK initialization version')
    require(len(sdk['outputs']) == n, 'SDK output count')
    compared = 0
    outputs = []
    for i, a in enumerate(sdk['outputs']):
        require(a['file'] == f"{header['motion_prefix']}.dlaa-{i}.rgba32f", 'SDK output filename')
        path = folder/a['file']
        require(path.stat().st_size == a['bytes'] == capture['width']*capture['height']*16, 'SDK output size')
        require(hashlib.sha256(path.read_bytes()).hexdigest() == a['sha256'], 'SDK output hash')
        rgb = np.fromfile(path, dtype='<f4').reshape(-1,4)[:,:3]
        require(np.isfinite(rgb).all() and not np.any(rgb == 65504), 'SDK finite/written pixels')
        source = folder/capture['owned_gpu_inputs']['sets'][i]['files'][0]['file']
        original = np.fromfile(source, dtype='<f4').reshape(-1,4)[:,:3]
        changed = int(np.count_nonzero(rgb != original))
        require(changed > len(rgb) and float(rgb.std()) > 0, 'SDK output changed/nonconstant')
        compared += int(rgb.size)
        outputs.append(dict(**a, rgb_values=int(rgb.size), changed_rgb_values=changed,
                            mean_absolute_difference=float(np.abs(rgb.astype(np.float64)-original).mean())))
    return dict(sdk_history_retained_verified=rows[0]['version'] in (4,5),sdk_projection_jitter_bound_verified=rows[0]['version'] in (4,5),
                sdk_evaluation_verified=True, live_offscreen_sdk_verified=True, sdk_rgb_values_checked=compared,
                sdk_outputs=outputs, sdk_shutdown_deferred=True, sdk_temporal_quality_verified=False, dlaa_ready=False,**startup_result)

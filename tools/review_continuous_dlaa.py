"""Validate a stopped continuous prototype's telemetry, not pixel quality."""
import argparse
import hashlib
import json
from pathlib import Path
from arm_continuous_dlaa import parse, require
from review_ngx_startup import review as review_startup
from continuous_history import review as review_history
from continuous_requests import review as review_requests
from continuous_retention import review as review_retention
from continuous_resolve import review as review_resolve
from continuous_forwarded import review as review_forwarded
from continuous_activation import review as review_activation
from continuous_reconstruction import review as review_reconstruction
from continuous_states import review as review_states
from continuous_admission import review as review_admission
from continuous_depth import review as review_depth
from continuous_scaled import review as review_scaled
from continuous_scaled_depth import review as review_scaled_depth
from continuous_depth_schedule import review as review_depth_schedule
from continuous_consumer_depth import review as review_consumer_depth
from continuous_readiness import review as review_readiness
from continuous_dependencies import review as review_dependencies, review_intermediate
from continuous_compute_dependencies import review as review_compute_dependencies
from continuous_output_diagnostics import review as review_output_diagnostics
from continuous_haze import review as review_haze
from continuous_prepared_queue import review as review_prepared_queue
from continuous_draw_state import review as review_draw_state


def review_shader_admission(status, previous=None, preparation=False):
    channel=status['preparation_shader_admission' if preparation else 'shader_admission']
    require(channel['schema']==1 and len(channel['variants'])==2,'shader admission schema')
    variants=channel['variants']
    for i,(technique,size) in enumerate(((23086,15492),(29842,15284))):
        v=variants[i]
        require(v['technique']==technique and v['bytes']==size,'shader variant identity')
        require(type(v['accepted']) is int and 0<=v['accepted']<2**64-1,'shader admission count')
        require(type(v['verified']) is bool and v['verified']==(v['accepted']>0),'shader verification coverage')
        if previous:require(v['accepted']>=previous['variants'][i]['accepted'],'shader admission rollback')
    accepted=sum(v['accepted'] for v in variants)
    if preparation:
        require(status['scaled_route']['published']<=accepted<=status['requests']['published'],'preparation shader accounting')
    else:
        require(accepted==status['resolve']['shader_checks'] and status['resolve']['shader_rejected']==0,'shader admission accounting')
    return channel


def review(rows, startup, expected_dll):
    require(rows and len(rows) >= 6, 'complete log')
    h = rows[0]
    parse((json.dumps(h) + '\n').encode('utf-8'))
    require(h['dll_sha256'] == expected_dll.lower(), 'DLL identity')
    require([r['event'] for r in rows[:4]] == ['start', 'reservation', 'ready', 'armed'], 'admission order')
    require(rows[1]['ready'] is True and rows[1]['slot'] > 0 and rows[2]['hooks_installed'] is False, 'readiness')
    require(rows[3]['installed'] is True and rows[3]['reason'] == '', 'installation')
    require(all(r['event'] == 'continuous' for r in rows[4:-1]), 'status events')
    require(rows[-1] == dict(event='stopped', armed=True, healthy=True, dll_pinned=True), 'clean stop')
    start = review_startup(startup)
    require(start['accepted'], 'startup')
    require(all(h[k] == startup[0][k] for k in ('version', 'pid', 'creation', 'base', 'dll_sha256', 'host_sha256')), 'same startup generation')
    previous_frames = previous_signal = 0
    prior_output_diagnostics = None
    prior_compute = None
    prior_haze = None
    prior_prepared_queue = None
    prior_preparation_shader = None
    prior_draw_state = None
    history_policy = {'1.18.1': 'v1.18.0-unchanged', '1.18.2': 'camera-displacement-v1', '1.18.3': 'camera-displacement-v1', '1.18.4': 'camera-displacement-v1'}.get(h['version'])
    resolve = h['version'] in ('1.19.0', '1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44')
    forwarded = h['version'] in ('1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44')
    activation = h['version'] in ('1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44')
    if resolve:
        history_policy = 'distance-only-comparison-v1' if h['version'] in ('1.25.12','1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 'camera-displacement-v1'
    retirement = h['version'] in ('1.18.4', '1.19.0', '1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44')
    prior_retention = None
    prior_resolve = None
    prior_forwarded = None
    prior_activation = None
    prior_reconstruction = None
    prior_states = None
    prior_admission = None
    prior_depth = None
    prior_scaled = None
    prior_scaled_depth = None
    prior_depth_schedule = None
    prior_consumer_depth = None
    prior_readiness = None
    previous_suppressed = previous_control_resets = 0
    prior_shader_admission = None
    prior_dependencies = None
    prior_intermediate = None
    prior_binding_checks = None
    prior_refresh = None
    for s in rows[4:-1]:
        if h["version"] in ("1.25.41", "1.25.42", "1.25.43"):require("dof_native_sequence" in s["scaled_route"],"native DOF sequence channel required")
        if h['version'] in ('1.25.43', '1.25.44'):require('silhouette_probe' in s['scaled_route'],'indexed silhouette channel required')
        if h['version']=='1.25.44':require('silhouette_output' in s['scaled_route'],'silhouette output channel required')
        if h['version'] in ('1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require('compute_depth_pixels' in s['scaled_route'],'depth pixel channel required')
        if h['version'] in ('1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['scaled_route']['dependency_route']['schema']==2 and s['scaled_route']['dependency_binding_checks']['schema']==2,'DOF binding/route schema required')
        if h['version'] in ('1.25.36','1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['scaled_route']['compute_dependencies']['schema']==3,'DOF constants required')
        if h['version'] in ('1.25.37','1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require('compute_resources' in s['scaled_route'],'compute resource channel required')
        if h['version']=='1.25.38':require(s['scaled_route']['compute_resources']['schema']==2,'compute entry diagnostics required')
        if h['version'] in ('1.25.39','1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['scaled_route']['compute_resources']['schema']==3,'stable compute recipe diagnostics required')
        if h['version'] in ('1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_compute=review_compute_dependencies(s['scaled_route']['compute_dependencies'],prior_compute)
        if h['version'] in ('1.25.33','1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            prior_intermediate=review_intermediate(s['scaled_route']['intermediate_dependencies'],prior_intermediate)
        if h['version'] in ('1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            from continuous_binding_checks import review as review_binding_checks
            prior_binding_checks=review_binding_checks(s['scaled_route']['dependency_binding_checks'],prior_binding_checks)
        if h['version'] in ('1.25.32','1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            from continuous_dependency_refresh import review as review_refresh
            prior_refresh=review_refresh(s['scaled_route']['dependency_refresh'],prior_refresh)
        if h['version'] in ('1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(s['scaled_route']['overlays']['policy']=='native-extent-and-dependencies-v2' and
                    s['scaled_route']['readiness']['schema']==4 and 'dependency_route' in s['scaled_route'],'dependency renderer telemetry required')
        if h['version'] in ('1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            prior_dependencies=review_dependencies(s['scaled_route']['dependencies'],prior_dependencies)
        if h['version'] in ('1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(all(k in s['scaled_route']['overlays'] for k in ('shared_key_prepared','shared_key_completed')),'shared-key telemetry required')
        if h['version'] in ('1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(s['scaled_route']['diagnostics'].get('vertex_policy')=='native-overlay-layout-v1','native overlay layout policy required')
            require('extended_draws' in s['scaled_route']['overlays'] and 'extended_layout' in s['scaled_route']['overlays'],'extended layout telemetry required')
        if h['version'] in ('1.25.24','1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            diagnostic=s['scaled_route']['diagnostics']
            graphs=diagnostic['graphs']+([diagnostic['graph_baseline']] if diagnostic['graph_baseline'] is not None else [])
            require(all('vertex_gate' in g for g in graphs),'vertex gate receipts required')
        if h['version'] in ('1.25.16','1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_prepared_queue=review_prepared_queue(s,prior_prepared_queue)
        require(s['schema'] == 1 and s['fault'] == 0, 'runtime fault')
        if h['version'] in ('1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_shader_admission=review_shader_admission(s,prior_shader_admission)
        if h['version'] in ('1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['shader_rejection']['schema']==1 and s['shader_rejection']['present'] is False,'unexpected native shader rejection')
        for k in ('frames', 'depth_copies', 'signal_value', 'completed_value', 'skips', 'history_resets', 'jitter_injections', 'jitter_restorations', 'list_count', 'resource_count', 'pending'):
            require(type(s[k]) is int and 0 <= s[k] < 2**64-1, 'counter ' + k)
        require(previous_frames <= s['frames'] <= s['depth_copies'], 'frame monotonicity')
        signal_bound=s['depth_copies']+(s['spatial_recordings']+s['route_only_signals'] if h['version'] in ('1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 0)
        require(previous_signal <= s['signal_value'] <= signal_bound, 'signal monotonicity')
        require(s['completed_value'] <= s['signal_value'], 'completed fence range')
        require(s['list_count'] <= 128 and s['resource_count'] <= (512 if retirement else 128), 'bounded registries')
        require(s['history_resets'] <= s['frames'], 'reset count')
        require((s['width'], s['height'], s['format']) == (start['width'], start['height'], start['swapchain_format']), 'graphics identity')
        require([s['luid_high'], s['luid_low']] == start['adapter_luid'], 'adapter identity')
        p = s['sdk']
        if history_policy:
            review_history(s['history'], s['frames'], s['history_resets'], history_policy)
            if history_policy == 'distance-only-comparison-v1':
                current=s['history']
                require(current['displacement_only_resets_suppressed'] >= previous_suppressed and current['control_resets'] >= previous_control_resets, 'history comparison monotonicity')
                previous_suppressed=current['displacement_only_resets_suppressed'];previous_control_resets=current['control_resets']
        if h['version'] in ('1.18.3', '1.18.4', '1.19.0', '1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            review_requests(s['requests'], s['fault'])
            require(s['frames'] <= s['requests']['published'] and s['frames'] <= s['requests']['taa']['matched'], 'evaluated request coverage')
        if retirement:
            r = review_retention(s)
            if prior_retention:
                require(all(r[k] >= prior_retention[k] for k in ('created', 'submitted', 'retired', 'released', 'peak')), 'retention counter rollback')
            prior_retention = r
        if resolve:
            prior_resolve = review_resolve(s, prior_resolve)
        if forwarded:
            prior_forwarded = review_forwarded(s, prior_forwarded)
        if activation:
            if h['version'] in ('1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['activation']['schema']==2,'postqueue activation schema')
            prior_activation = review_activation(s, prior_activation)
        if h['version'] in ('1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            prior_reconstruction=review_reconstruction(s,prior_reconstruction)
            require(not prior_reconstruction['feature_failed'],'latched failure is not a successful DLAA run')
        if h['version'] in ('1.21.2','1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['state_trace']['schema']==2,'native provenance schema')
        if h['version'] in ('1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_states=review_states(s,prior_states)
        if h['version'] in ('1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_admission=review_admission(s,prior_admission)
        if h['version'] in ('1.21.5','1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require('depth_destination' in s,'destination channel missing')
        if h['version'] in ('1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_depth=review_depth(s,prior_depth,scaled=h['version'] in ('1.25.0','1.25.1','1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'))
        if h['version'] in ('1.25.3','1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_scaled_depth=review_scaled_depth(s,prior_scaled_depth)
        if h['version'] in ('1.25.4','1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_depth_schedule=review_depth_schedule(s,prior_depth_schedule)
        if h['version'] in ('1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(s['consumer_depth']['schema'] == (2 if h['version'] in ('1.25.15','1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 1), 'consumer depth version/schema')
            prior_consumer_depth=review_consumer_depth(s,prior_consumer_depth)
        if h['version'] in ('1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_readiness=review_readiness(s,prior_readiness)
        if h['version'] in ('1.25.7','1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_output_diagnostics=review_output_diagnostics(s,prior_output_diagnostics)
        if h['version'] in ('1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['scaled_route']['diagnostics'].get('command_policy')=='prepared-command-rejections-v1' and 'commands' in s['scaled_route']['diagnostics'],'prepared command diagnostics required')
        if h['version'] in ('1.25.19','1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            prior_preparation_shader=review_shader_admission(s,prior_preparation_shader,preparation=True)
            require('failure_diagnostics' in s['consumer_depth'],'depth failure diagnostics missing')
        if h['version'] in ('1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require('overlays' in s['scaled_route'] and 'recording_states' in s['scaled_route'],'overlay/state channels required')
        if h['version'] in ('1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(s['consumer_depth']['copy_execution_policy']=='native-opcode10-branches-v1','native copy execution missing')
            prior_draw_state=review_draw_state(s,prior_draw_state)
            require(prior_draw_state['count']==0,'output-state rejection')
        if h['version'] in ('1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['scaled_route']['abandoned'] is False,'abandoned frame route')
        if h['version'] in ('1.25.10','1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(s['scaled_route']['schema']==(3 if h['version'] in ('1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 2) and s['scaled_route']['readiness']['schema']==(4 if h['version'] in ('1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 3 if h['version'] in ('1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 2) and s['scaled_route']['diagnostics']['schema']==2,'four-effect route telemetry schema')
        if h['version'] in ('1.25.8','1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(s['scaled_route']['haze_capture']['schema']==(2 if h['version'] in ('1.25.9','1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 1),'haze capture schema')
            prior_haze=review_haze(s,prior_haze)
        require(p['continuous'] is True and p['creation_completed'] is True and p['create_result'] == 1, 'SDK creation')
        require(p['evaluations'] == s['frames'] and p['resets'] == s['history_resets'], 'SDK counter agreement')
        require(0 < p['allocation_bytes'] <= 419430400 and p['captures'] == 0, 'streaming allocations/readbacks')
        require(p['create_flags'] == 75 and p['mode'] == (0 if h['version'] in ('1.25.0','1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 5) and p['preset'] == 11, 'SDK mode')
        require(p['input_formats'] == [26, 41, 42] and p['converted_formats'] == [2, 41, 16], 'formats')
        require(p['mv_scale'] == [1, 1] and p['mv_jittered'] is False and p['pre_exposure'] == 1, 'motion/exposure')
        if s['frames']:
            require(p['evaluate_result'] == 1 and p['failure'] == '' and p['copied'] is True and p['restored'] is True, 'SDK evaluation/handoff')
            require(p['command'] > 0 and p['native_output'] > 0 and p['generation'] > 0, 'native recording')
            require(0 < p['frame_ms'] <= 1000 and all(-.5 <= j <= .5 for j in p['jitter']), 'frame parameters')
        if h['version'] in ('1.25.1','1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require('final_captured' in s['scaled_route'] and 'final_rejected' in s['scaled_route'],'final quad provenance missing')
        if h['version'] in ('1.25.0','1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):prior_scaled=review_scaled(s,prior_scaled)
        previous_frames, previous_signal = s['frames'], s['signal_value']
    final = rows[-2]
    require(final['frames'] > 9, 'sustained run')
    require(final['active'] is False and final['hooks_disabled'] is True and final['pending'] == 0, 'callback closure')
    require(final['pending_private_recording'] is False, 'no pending private recording')
    expected_signals=final['depth_copies']+(final['spatial_recordings']+final['route_only_signals'] if h['version'] in ('1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 0)
    require(final['signal_value'] == final['completed_value'] == expected_signals, 'all private submissions completed')
    require(final['jitter_injections'] == final['jitter_restorations'], 'all jitter restored')
    policy = 'reset-and-fence-v1' if retirement else 'bounded-process-lifetime'
    require(final['retention'] == h['retention'] == policy and final['sdk']['released'] is False, 'explicit lifetime policy')
    if h['version'] in ('1.25.0','1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):review_scaled(final,final=True)
    result = dict(accepted=True, version=h['version'], pid=h['pid'], creation=h['creation'],
                continuous_path_telemetry_verified=True, live_pixel_readback_verified=False,
                temporal_quality_verified=False, full_transition_stability_verified=False,
                frames=final['frames'], skips=final['skips'], history_resets=final['history_resets'],
                gpu_signals_completed=final['completed_value'], jitter_restored=final['jitter_restorations'],
                hooks_disabled=True, fault=0, pending=0, list_count=final['list_count'], resource_count=final['resource_count'])
    if history_policy:
        result['history'] = review_history(final['history'], final['frames'], final['history_resets'], history_policy)
    if h['version'] in ('1.18.3', '1.18.4', '1.19.0', '1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
        result['requests'] = review_requests(final['requests'], final['fault'])
    if retirement:
        result['recordings'] = review_retention(final)
    if resolve:
        result['resolve'] = review_resolve(final, final=True)
    if forwarded:
        result['forwarded_requests'] = review_forwarded(final, final=True)
    if activation:
        result['activation'] = review_activation(final, final=True)
    if h['version'] in ('1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['reconstruction_control']=review_reconstruction(final,final=True)
    if h['version'] in ('1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['state_trace']=review_states(final)
    if h['version'] in ('1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['state_admission']=review_admission(final)
    if h['version'] in ('1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['depth_eligibility']=review_depth(final,scaled=h['version'] in ('1.25.0','1.25.1','1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'))
    if h['version'] in ('1.25.0','1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
        result['scaled_route']=review_scaled(final,final=True)
        result['render_extent']=[final['render_width'],final['render_height']]
        result['output_extent']=[final['width'],final['height']]
        result['full_render_scale_restored']=True
    if h['version'] in ('1.25.3','1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['scaled_depth_capture']=review_scaled_depth(final)
    if h['version'] in ('1.25.4','1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['depth_schedule']=review_depth_schedule(final)
    if h['version'] in ('1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['consumer_depth']=review_consumer_depth(final)
    if h['version'] in ('1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['readiness']=review_readiness(final)
    if h['version'] in ('1.25.7','1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['output_diagnostics']=review_output_diagnostics(final)
    if h['version'] in ('1.25.8','1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['haze_capture']=review_haze(final, final=True)
    if h['version'] in ('1.25.33','1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['intermediate_dependencies']=prior_intermediate
    if h['version'] in ('1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):result['compute_dependencies']=prior_compute
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log', type=Path)
    ap.add_argument('--startup', type=Path, required=True)
    ap.add_argument('--dll-sha256', required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    require(not a.output.exists(), 'preserve review')
    raw = a.log.read_bytes()
    result = review(parse(raw), [json.loads(line) for line in a.startup.read_text(encoding='utf-8').splitlines()], a.dll_sha256)
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    with a.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

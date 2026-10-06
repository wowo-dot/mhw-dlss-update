"""Generation-checked controls and bounded final-target API observations; no FG activation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
from reflex_frames import check, control, HOST_SHA
from frame_generation_ownership import review as ownership_review

VERSION = '0.6.5'
TRACE_POLICY = 'submitted-scene-depth-presentation-map-v5'
REASONS = 24
DRAW_REASONS = set(range(13)) | set(range(16, 21))
COPY_REASONS = {13, 14}
DEPTH_REASONS = set(range(16, 20))
U64 = 0xffffffffffffffff


def identity(rows):
    h = rows[0]
    check(h.get('event') == 'start' and h.get('schema') == 7 and h.get('version') == VERSION and
          h.get('experiment') == 'frame-generation-scene-boundary', 'scene identity')
    check(h.get('host_sha256') == HOST_SHA and h.get('fg_enabled') is False and
          h.get('fg_plugin_loaded') is False and h.get('temporal_version') == '1.25.44' and
          h.get('upscaling') is True, 'scene scope')
    return h


def positive(value):
    return type(value) is int and value > 0


def bounded(values, length, limit=U64):
    return type(values) is list and len(values) == length and all(type(n) is int and 0 <= n <= limit for n in values)


def raster_metadata(e, qualified=False):
    cache = e.get('native_cache', [])
    mask = e.get('native_cache_read_mask')
    r = e.get('raster_observation', [])
    check(bounded(cache, 11) and type(mask) is int and 0 <= mask <= 0x7ff and
          all(mask & (1 << i) or value == 0 for i, value in enumerate(cache)), 'bounded native cache read bits')
    check(bounded(r, 19) and
          all(r[i] in (0, 1) for i in (0,1,2,3,4,5,10,13,14,16,17)) and
          r[8] <= 63 and r[9] <= 0xffffffff and r[11] <= 0xffffffff and r[12] <= 0xffffffff,
          'bounded raster/OM observation')
    if not r[0]:
        check(r == [0]*19, 'absent observer entry')
    else:
        check(positive(r[6]) and r[6] == r[7] and (not r[8] & 1 or r[9] & 1), 'raster generation/vertex mask')
        check((r[10] or r[11:] == [0]*8) and
              (not r[14] or r[13] and r[11] > 0) and (r[14] or r[15] == 0) and
              (not r[17] or r[16]) and (r[17] or r[18] == 0), 'returned OM handle observations')
        check(bool(r[8] & 32) == bool(r[10] and r[11] == 1 and not r[16] and r[14]),
              'unchanged single-RTV/no-depth admission')
    missing = [name for bit, name in ((1,'vertex'),(2,'viewport'),(4,'scissor'),(8,'topology')) if not r[8] & bit]
    blockers = []
    if not r[0]:blockers.append('entry-absent')
    if not all(r[i] for i in (1,2,3)):blockers.append('observer-unavailable')
    if not r[4] or not r[5] or not r[6]:blockers.append('bindings-invalid')
    blockers.extend('raster-'+name+'-unknown' for name in missing)
    if not r[8] & 32:blockers.append('single-RTV-output-unknown')
    # The separate v0.6.5 diagnostic qualification: complete raster fields,
    # one readable RTV and one readable depth descriptor. Never an admission.
    depth_output = (r[0] == 1 and all(r[i] == 1 for i in (1,2,3,4,5)) and positive(r[6]) and r[6] == r[7] and
                    r[8] & 15 == 15 and not r[8] & 32 and r[10] == 1 and r[11] == 1 and r[13] == 1 and r[14] == 1 and
                    positive(r[15]) and r[16] == 1 and r[17] == 1 and positive(r[18]))
    if qualified:
        check(mask == 0x7ff and r[6] == e['generation'] and
              cache[1] == e['list'] and cache[2] == e['packet'] and cache[3] == cache[4] and
              cache[5] & 7 == 1 and cache[8:11] == [e['rtv'],e['root'],e['pipeline']] and
              r[15] == e['rtv'], 'qualified native/raster snapshots')
        if qualified == 'depth':
            check(depth_output and r[18] == e['dsv'], 'diagnostic depth-bearing single-RTV observation')
        else:
            check(not blockers, 'preserved single-RTV/no-depth observation')
    return dict(blockers=blockers, missing_raster_fields=missing, OM_seen=bool(r[10]),
                render_target_count=r[11], contiguous_range_raw=r[12],
                RTV_argument_present=bool(r[13]), RTV_read=bool(r[14]), RTV_handle=r[15],
                depth_argument_present=bool(r[16]), depth_read=bool(r[17]), depth_handle=r[18],
                depth_output_qualifiable=depth_output,
                native_cache_read_mask=mask, diagnostics_authorize_rendering=False)


def output_metadata(e):
    o = e.get('output_native', [])
    mask = e.get('output_native_read_mask')
    facts = e.get('pipeline_facts', [])
    check(type(e.get('diagnostic')) is bool and type(e.get('presentation_label')) is bool and
          e.get('output_scope') in (0, 1) and type(e.get('dsv')) is int and 0 <= e['dsv'] <= U64 and
          bounded(o, 8) and type(mask) is int and 0 <= mask <= 0xff and
          all(mask & (1 << i) or value == 0 for i, value in enumerate(o)) and
          bounded(facts, 14, 0xffffffff), 'bounded output/pipeline metadata')
    check(not e['presentation_label'] or e['diagnostic'], 'presentation labels are diagnostic')
    check(e['output_scope'] == 0 or (e['diagnostic'] and e['kind'] in (0, 1)), 'depth scope is diagnostic draw metadata')
    if e['output_scope'] == 0:
        check(e['dsv'] == 0 and mask == 0 and o == [0]*8, 'no depth attachment metadata outside depth scope')
    return o, mask, facts


def buffers_metadata(b):
    check(type(b) is dict and all(type(b.get(k)) is int and 0 <= b[k] <= U64 for k in
          ('swap','generation','conflicts','out_of_range','foreign','known_mask','game_index_calls',
           'game_index_present','game_index','presents','present_index')) and
          bounded(b.get('labels'), 16) and b['known_mask'] < 1 << 16 and
          all(bool(b['known_mask'] & (1 << i)) == (value != 0) for i, value in enumerate(b['labels'])) and
          len({v for v in b['labels'] if v}) == bin(b['known_mask']).count('1'), 'presentation buffer metadata')
    return {v for v in b['labels'] if v}


def submitted(e, f):
    return e['returned'] is True and positive(e['batch']) and e['qpc'] <= e['submit_qpc'] <= f['present_qpc']


def receipt(e):
    check(all(type(e.get(k)) is int and e[k] >= 0 for k in ('batch','batch_index','batch_count','submit_qpc','queue')),
          'receipt fields')
    if e['batch']:
        check(positive(e['queue']) and e['qpc'] <= e['submit_qpc'] and
              0 <= e['batch_index'] < e['batch_count'] <= 128, 'submitted recording receipt')
    else:
        check(e['queue'] == e['submit_qpc'] == e['batch_index'] == e['batch_count'] == 0, 'unsubmitted receipt')


def presentation_mapping(f, request_labels):
    q, p = f['request_buffers'], f['present_buffers']
    present_labels = buffers_metadata(p)
    index = p['present_index']
    label = p['labels'][index] if index < 16 and p['known_mask'] & (1 << index) else 0
    # Request-time labels seed matching; they count only when Present sees
    # the same chain generation and labels. Equality is not ownership proof.
    valid = (bool(request_labels) and q['swap'] == p['swap'] == f['swap'] and q['generation'] == p['generation'] and
             positive(p['generation']) and q['labels'] == p['labels'] and p['conflicts'] == q['conflicts'] and
             positive(p['presents']) and positive(label))
    return dict(valid=valid, label=label if valid else 0, index=index, generation=p['generation'],
                game_index_agrees=valid and p['game_index'] == index and p['game_index_present'] + 1 == p['presents'],
                labels_known=len(present_labels))


def event_summary(e):
    return dict(ordinal=e['ordinal'], kind=e['kind'], resource=e['resource'], output_scope=e['output_scope'],
                presentation_label=e['presentation_label'], shader=e['shader'], pipeline=e['pipeline'],
                blend_RT0=e['pipeline_facts'][:10], depth_state=e['pipeline_facts'][10:13],
                DSV_format=e['pipeline_facts'][13], copy=e['copy'][:2] if e['kind'] == 4 else None,
                before=e['before'] if e['kind'] == 2 else None, after=e['after'] if e['kind'] == 2 else None)


def scene_review(trace, image, owner, buffers=()):
    check(trace.get('schema') == 5 and trace.get('policy') == TRACE_POLICY and
          trace.get('diagnostic_only') is True and
          all(trace.get(k) is False for k in ('gpu_work_added', 'pointers_owned', 'pixel_contents_verified', 'GPU_completion_verified')),
          'scene metadata contract')
    check(trace.get('capacity') == 4 and trace.get('event_capacity') == 1024 and trace.get('diagnostic_capacity') == 512 and
          trace.get('pending') == 0 and trace.get('resource_capacity') == 16 and trace.get('rejection_reasons') == REASONS and
          trace.get('samples_per_reason') == 2 and trace.get('presentation_buffer_capacity') == 16, 'bounded completed requests')
    captures = trace.get('captures', [])
    check(0 < len(captures) == trace.get('requested') <= 4, 'requested capture count')
    result = []
    for request, f in enumerate(captures, 1):
        check(f.get('request') == request and f.get('presented') is True and positive(f.get('reflex_id')) and
              f.get('present_thread') == owner and positive(f.get('swap')) and positive(f.get('present_qpc')) and
              positive(f.get('present_frame')) and type(f.get('present_epoch')) is int and f['present_epoch'] >= 0, 'frame present linkage')
        v = f.get('identity', [])
        check(len(v) == 11 and all(type(x) is int and x >= 0 for x in v) and
              all(positive(v[i]) for i in (0, 2, 3, 4, 5, 6, 7, 8)) and v[5] != v[6] and v[10] < 2,
              'frame/resource metadata identity')
        check(f.get('overflow') == f.get('duplicate_submissions') == f.get('premature_resets') == 0,
              'untruncated unique preserved observations')
        check(f.get('choice') in (1, 2, 3), 'sealed rendering choice')
        counters = ('events','diagnostic_events','diagnostic_stored','diagnostic_overflow','diagnostic_duplicate_submissions',
                    'diagnostic_premature_resets','diagnostic_unreturned_at_present','diagnostic_unsubmitted_at_present')
        check(all(type(f.get(k)) is int and f[k] >= 0 for k in counters), 'diagnostic counters')
        request_labels = buffers_metadata(f.get('request_buffers'))
        obs = f.get('observations', [])
        diagnostic = [e for e in obs if e.get('diagnostic') is True]
        check(0 < len(obs) == f['events'] - f['diagnostic_overflow'] <= 1024 and
              len(diagnostic) == f['diagnostic_stored'] <= 512 and
              f['diagnostic_events'] == f['diagnostic_stored'] + f['diagnostic_overflow'], 'bounded event count')
        resources = f.get('followed_resources', [])
        check(0 < len(resources) <= 16 and resources[0] == v[6] and all(positive(n) for n in resources) and
              len(set(resources)) == len(resources), 'bounded unique followed labels')
        previous = 0
        recordings, batch_slots, batches = {}, {}, {}
        copy_edges = []
        resource_overflow = unreturned = unsubmitted = 0
        for ordinal, e in enumerate(obs, 1):
            check(e.get('ordinal') == ordinal and e.get('kind') in (0, 1, 2, 3, 4), 'observation ordering/kind')
            check(positive(e.get('qpc')) and previous <= e['qpc'] <= f['present_qpc'], 'observation clock')
            check(positive(e.get('native_frame')) and type(e.get('native_epoch')) is int and e['native_epoch'] >= 0,
                  'separate native recording clock')
            previous = e['qpc']
            o, omask, _ = output_metadata(e)
            receipt(e)
            check(positive(e.get('list')) and positive(e.get('generation')) and positive(e.get('thread')) and
                  type(e.get('returned')) is bool, 'recording identity')
            if e['diagnostic']:
                check(e.get('stage') == 0xffffffff, 'diagnostic events are never the final stage')
                check((e['presentation_label'] and e['resource'] in request_labels) or
                      (not e['presentation_label'] and e['resource'] in resources), 'diagnostic resource class')
                if not e['returned']:unreturned += 1
                if not e['batch'] or e['submit_qpc'] > f['present_qpc']:unsubmitted += 1
            else:
                check(e['resource'] in resources and e['returned'] is True, 'target/recording return')
                check(submitted(e, f), 'preserved submitted recording receipt')
            if e['batch']:
                key = e['list'], e['generation']
                location = e['batch'], e['queue'], e['batch_index'], e['batch_count'], e['submit_qpc']
                check(key not in recordings or recordings[key] == location, 'one submission per recording generation')
                recordings[key] = location
                batch_key = e['batch'],e['batch_index']
                check(batch_key not in batch_slots or batch_slots[batch_key] == key, 'unique submitted list position')
                batch_slots[batch_key] = key
                batch_location = e['queue'],e['batch_count'],e['submit_qpc']
                check(e['batch'] not in batches or batches[e['batch']] == batch_location, 'one batch receipt')
                batches[e['batch']] = batch_location
            if e['kind'] in (0, 1):
                check(e.get('api_state_matched') is True and positive(e.get('rtv')) and
                      positive(e.get('root')) and positive(e.get('pipeline')) and positive(e.get('packet')) and
                      image <= e.get('caller', 0) < image + 0x55d3000, 'native draw/API binding identity')
                check(len(e.get('args', [])) == 5 and positive(e['args'][0]) and positive(e['args'][1]), 'draw arguments')
                if e['output_scope'] == 1:
                    raster_metadata(e, qualified='depth')
                    check(omask == 0xff and o[0] == e['rtv'] and positive(o[1]) and positive(o[2]) and
                          o[3] == o[6] == e['dsv'] and positive(o[4]) and o[7] == o[4] and o[5] <= 0xffffffff and
                          e['packet'] != v[4], 'native depth attachment agreement')
                else:
                    raster_metadata(e, qualified=True)
            else:
                raster_metadata(e)
                check(e['native_cache_read_mask'] == 0 and e['native_cache'] == [0]*11 and
                      e['raster_observation'] == [0]*19, 'non-draw raster metadata absent')
            if e['kind'] == 4:
                cp = e.get('copy', [])
                check(bounded(cp, 16) and positive(cp[0]) and positive(cp[1]) and cp[2] == cp[3] == 0 and
                      all(n <= 0xffffffff for n in cp[4:]) and cp[9] in (0, 1) and
                      e['resource'] in cp[:2] and
                      image <= e.get('caller', 0) < image+0x55d3000, 'native texture copy')
                check((cp[9] == 0 and cp[10:] == [0]*6) or
                      (cp[9] == 1 and all(cp[n] < cp[n+3] for n in (10, 11, 12))), 'copy box extent')
                if e['diagnostic']:
                    check(e['presentation_label'] and cp[0] not in resources and cp[1] not in resources and
                          (cp[0] in request_labels or cp[1] in request_labels), 'presentation-only copy')
                else:
                    check(e.get('replaced') is False, 'returned native texture copy')
                    copy_edges.append(cp[:2])
                    if cp[0] in resources and cp[1] not in resources:resource_overflow += 1
        check(f['diagnostic_unreturned_at_present'] == unreturned and
              f['diagnostic_unsubmitted_at_present'] == unsubmitted, 'diagnostic closure accounting')
        # Native API returns on different workers can reverse entry order.
        # Validate label reachability without equating entry/return/GPU order.
        reachable = {v[6]}
        for _ in range(len(copy_edges)+1):
            old = len(reachable)
            for src,dst in copy_edges:
                if src in reachable:reachable.add(dst)
            if len(reachable) == old:break
        check(set(resources) <= reachable and f.get('resource_overflow') == resource_overflow and
              ((resource_overflow == 0 and set(resources) == reachable) or
               (resource_overflow > 0 and len(resources) == 16)), 'bounded returned-copy resource paths')
        final = [e for e in obs if e.get('stage') == 2]
        check(len(final) == 1 and final[0].get('kind') == 0 and final[0].get('packet') == v[4] and
              final[0].get('replaced') is True and final[0].get('pipeline_read') is True and not final[0]['diagnostic'] and
              final[0].get('rotation') == 0 and final[0]['resource'] == v[6], 'qualified final-screen draw')
        # CPU-submitted order on one queue only: batch, list position, then
        # within-list callback order. Never a GPU completion claim.
        complete = [e for e in obs if submitted(e, f)]
        one_queue = len({e['queue'] for e in complete}) == 1
        gpu = sorted(complete, key=lambda e: (e['batch'], e['batch_index'], e['ordinal']))
        after = gpu[gpu.index(final[0])+1:] if one_queue else []
        draws = [e for e in obs if e['kind'] in (0, 1) and not e['diagnostic']]
        by_gpu = sorted(draws, key=lambda e: (e['batch'], e['batch_index'], e['ordinal']))
        counts = f.get('rejection_counts', [])
        samples = f.get('rejection_samples', [])
        check(bounded(counts, REASONS) and all(counts[n] == 0 for n in (15, 21, 22, 23)) and
              len(samples) == sum(min(n, 2) for n in counts), 'bounded rejection accounting')
        sampled = [0]*REASONS
        rejected_raster = []
        sample_previous = 0
        for e in samples:
            reason = e.get('reason')
            check(type(reason) is int and 0 <= reason < REASONS and e.get('kind') in (0, 1, 4), 'rejected entry kind/reason')
            check((reason in DRAW_REASONS and e['kind'] in (0,1)) or (reason in COPY_REASONS and e['kind'] == 4), 'entry reason family')
            sampled[reason] += 1
            check(positive(e.get('qpc')) and sample_previous <= e['qpc'] <= f['present_qpc'] and
                  positive(e.get('thread')) and positive(e.get('list')) and
                  positive(e.get('native_frame')) and type(e.get('native_epoch')) is int and e['native_epoch'] >= 0 and
                  image <= e.get('caller', 0) < image+0x55d3000, 'rejected entry snapshot')
            sample_previous = e['qpc']
            raster = raster_metadata(e)
            output_metadata(e)
            if reason == 9:
                # The probe also requires the current binding generation.
                eligible = raster['depth_output_qualifiable'] and e['raster_observation'][6] == e['generation']
                check(raster['blockers'] and not eligible,
                      'reason-9 has neither preserved nor depth-bearing complete output')
                rejected_raster.append(dict(qpc=e['qpc'], **raster))
            if reason in DEPTH_REASONS:
                check(raster['depth_output_qualifiable'] and e['output_scope'] == 1, 'depth-path rejection scope')
            for label in ('generation','packet','resource','rtv','root','pipeline','batch','queue','submit_qpc','batch_index','batch_count'):
                check(type(e.get(label)) is int and e[label] >= 0, 'rejected entry labels')
            if e['batch']:
                check(positive(e['generation']) and positive(e['queue']) and e['qpc'] <= e['submit_qpc'] <= f['present_qpc'] and
                      0 <= e['batch_index'] < e['batch_count'] <= 128, 'optional rejection submission')
            else:
                check(e['queue'] == e['submit_qpc'] == e['batch_index'] == e['batch_count'] == 0, 'unsubmitted snapshot')
        check(sampled == [min(n, 2) for n in counts], 'reason sample limits')
        unreadable = f.get('unreadable')
        outside = f.get('outside_scope')
        check(type(unreadable) is int and unreadable >= 0 and type(outside) is int and outside >= 0 and
              type(f.get('unmatched')) is int and f['unmatched'] >= 0, 'scope counter accounting')
        check(outside == counts[0] and unreadable == sum(counts[1:13]) + sum(counts[16:21]), 'draw rejection totals')
        ordered = {v[6]}
        ordered_copies = []
        for e in after:
            if e['kind'] == 4 and e['copy'][0] in ordered:
                ordered.add(e['copy'][1]);ordered_copies.append(e['copy'][:2])
        mapping = presentation_mapping(f, request_labels)
        presented = mapping['label']
        depth_draws = [e for e in obs if e['output_scope'] == 1]
        depth_complete = [e for e in depth_draws if submitted(e, f)]
        on_presented = [e for e in after if presented and
                        (e['resource'] == presented or (e['kind'] == 4 and presented in e['copy'][:2]))]
        result.append(dict(request=request, frame=v[0], epoch=v[1], reflex_frame=f['reflex_id'],
                           final_draw_native_frame=final[0]['native_frame'], final_draw_native_epoch=final[0]['native_epoch'],
                           present_native_frame=f['present_frame'], present_native_epoch=f['present_epoch'],
                           close_contract='exact-final-draw-returned-and-submitted-before-qualified-Present',
                           target=v[6], engine_extent=v[7:9], engine_GPU_format=v[9],
                           target_interface_seen_in_GetBuffer=v[6] in buffers,
                           draws=len(draws), post_final_draws=len(by_gpu)-by_gpu.index(final[0])-1 if one_queue else None,
                           same_queue_order=one_queue, complete_direct_draw_scope=unreadable == outside == 0,
                           unreadable=unreadable, outside_scope=outside,
                           followed_resources=resources, copies=sum(e['kind'] == 4 and not e['diagnostic'] for e in obs),
                           same_queue_post_final_copy_edges=ordered_copies if one_queue else None,
                           resource_capacity_exhausted=resource_overflow > 0,
                           diagnostic_events=f['diagnostic_events'], diagnostic_overflow=f['diagnostic_overflow'],
                           diagnostic_duplicate_submissions=f['diagnostic_duplicate_submissions'],
                           diagnostic_premature_resets=f['diagnostic_premature_resets'],
                           diagnostic_unreturned_at_present=unreturned, diagnostic_unsubmitted_at_present=unsubmitted,
                           depth_bearing_draws=len(depth_draws), depth_bearing_returned_submitted=len(depth_complete),
                           post_final_depth_bearing_draws=[event_summary(e) for e in after if e['output_scope'] == 1] if one_queue else None,
                           depth_bearing_shaders=sorted({e['shader'] for e in depth_complete}),
                           presentation_mapping_valid=mapping['valid'], presented_index=mapping['index'],
                           presented_label=presented, presentation_generation=mapping['generation'],
                           game_index_agrees_with_present_entry=mapping['game_index_agrees'],
                           presentation_labels_known=mapping['labels_known'],
                           post_final_events_on_presented_buffer=[event_summary(e) for e in on_presented] if one_queue else None,
                           ordered_copy_reaches_presented_buffer=bool(presented and presented in ordered) if one_queue else None,
                           ordered_copy_reaches_sampled_GetBuffer=bool((ordered-{v[6]}) & set(buffers)) if one_queue else None,
                           rejection_counts=counts, rejected_entry_samples=len(samples),
                           reason9_raster_observations=rejected_raster,
                           rejection_snapshots_prove_draw_completion=False,
                           copy_subresource_extents_verified=False, current_backbuffer_verified=False,
                           UI_semantics_verified=False, input_lifetime_verified=False,
                           diagnostics_authorize_rendering=False))
    return dict(captures=result, pixel_contents_verified=False, GPU_completion_verified=False,
                final_HUDless_color_verified=False, game_FG_activated=False)


def review(rows, inputs, startup, dll):
    h = identity(rows)
    normalized = [dict(r) for r in rows]
    normalized[0].update(schema=4, version='0.6.0', experiment='frame-generation-game-ownership')
    joined = ownership_review(normalized, inputs, startup, dll)
    last = [r for r in inputs if r.get('event') == 'continuous'][-1]
    buffers = {r['returned'] for r in rows if r.get('event') == 'ownership_sample' and
               r.get('method') == 5 and r.get('result') == 0 and r.get('returned')}
    owner = next(r['owner'] for r in rows if r.get('event') == 'frame_finished')
    image = next(r['image'] for r in rows if r.get('event') == 'ownership_summary')
    scene = scene_review(last['scaled_route']['scene_boundary'], image, owner, buffers)
    requests = [r for r in rows if r.get('event') == 'scene_capture_requested']
    check([r.get('request') for r in requests] == list(range(1, len(scene['captures'])+1)) and
          all(r.get('generation_checked') is True and type(r.get('presentation_buffers_read')) is bool and
              type(r.get('presentation_generation')) is int and type(r.get('presentation_known_mask')) is int
              for r in requests), 'capture request acknowledgements')
    return dict(schema=1, accepted=True, version=h['version'], scene=scene, renderer=joined)


def capture_control(rows, path, request):
    h = identity(rows)
    check(request in (2, 3, 4) and any(r.get('event') == 'frames_armed' for r in rows) and
          not any(r.get('event') in ('failed', 'worker_failed', 'finished', 'frame_finished') for r in rows), 'active scene recorder')
    check([r.get('request') for r in rows if r.get('event') == 'scene_capture_requested'] == list(range(1, request)),
          'sequential acknowledged requests')
    k = c.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; k.OpenProcess.restype = w.HANDLE
    k.GetProcessTimes.argtypes = [w.HANDLE]+[c.POINTER(w.FILETIME)]*4
    k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
    k.CloseHandle.argtypes = [w.HANDLE]
    process = k.OpenProcess(0x1000, False, h['pid']); check(process, 'process query')
    try:
        times = [w.FILETIME() for _ in range(4)]
        check(k.GetProcessTimes(process, *map(c.byref, times)), 'process identity')
        check((times[0].dwHighDateTime << 32 | times[0].dwLowDateTime) == h['creation'] and
              not(times[1].dwHighDateTime or times[1].dwLowDateTime), 'live process generation')
        text = c.create_unicode_buffer(32768); size = w.DWORD(len(text))
        check(k.QueryFullProcessImageNameW(process, 0, text, c.byref(size)), 'image path')
        check(hashlib.sha256(Path(text.value).read_bytes()).hexdigest() == HOST_SHA, 'retail identity')
    finally:
        k.CloseHandle(process)
    marker = Path(str(path)+'.scene-'+str(request))
    temporary = Path(str(marker)+'.tmp')
    check(not marker.exists(), 'preserve scene request')
    with temporary.open('x', encoding='ascii') as f:
        f.write(str(h['creation'])+'\n')
    # Windows rename fails when destination exists; publish only complete data.
    temporary.rename(marker)
    return dict(request=request, sent=True, acknowledged=False, pid=h['pid'], creation=h['creation'],
                process_access='PROCESS_QUERY_LIMITED_INFORMATION', process_memory_writes=0)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('arm', 'stop', 'capture', 'review'))
    p.add_argument('--log', type=Path, required=True); p.add_argument('--output', type=Path, required=True)
    p.add_argument('--request', type=int); p.add_argument('--dll-sha256')
    a = p.parse_args(); check(not a.output.exists() and a.log.stat().st_size <= 8*1024*1024, 'preserve bounded evidence')
    raw = a.log.read_bytes(); rows = [json.loads(r) for r in raw.splitlines()]; h = identity(rows)
    if a.action == 'review':
        check(a.dll_sha256, 'expected DLL')
        def paired(prefix):
            path = a.log.parent / f"{prefix}-{h['pid']}-{h['creation']}.jsonl"
            check(path.stat().st_size <= 64*1024*1024, 'bounded paired log')
            return [json.loads(r) for r in path.read_bytes().splitlines()]
        result = review(rows, paired('inputs'), paired('startup'), a.dll_sha256)
    elif a.action == 'capture':
        result = capture_control(rows, a.log, a.request)
    else:
        result = control(rows, a.log, a.action)
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    with a.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    # The full joined evidence belongs in the receipt, not the console.
    print(json.dumps({k: result[k] for k in ('accepted', 'version', 'request', 'action', 'pid', 'creation') if k in result}))


if __name__ == '__main__':
    main()

"""Verify AA-Off projection association and bindings, without a DLAA claim."""
import argparse
import json
from pathlib import Path
from arm_aa_boundary import parse, require
from review_aa_deferred import review as deferred_review
from review_aa_gpu import review_trace
from review_aa_inputs import review_images
from projection_contract import floats, matrices
from temporal_bindings import review_root, uint


def review_bindings(g):
    b = g['bindings']
    require(b['schema'] == 1 and b['fault'] == b['pending'] == 0 and b['hooks_disabled'] and b['drained'], 'binding closure')
    require(b['hook_count'] == 18 and 1 <= b['list_count'] <= 128 and len(b['records']) == 8, 'binding budget')
    for i,(r,s) in enumerate(zip(b['records'],g['records'])):
        require(r['receipt'] == s['receipt'] == i and r['frame'] == s['frame'] and r['command'] == s['command'], 'binding receipt identity')
        require(all(s[k] for k in ('binding_seen','binding_ready','binding_unchanged','binding_known','binding_valid')) and r['ready'], 'binding readiness')
        require(r['generation'] == s['reset'] == s['binding_generation'], 'binding recording generation')
        require(uint(r['pipeline'],positive=True) and r['pipeline'] == r['expected_pipeline'] == s['binding_pipeline'], 'binding pipeline')
        require(uint(r['compute']['signature'],positive=True) and r['compute']['signature'] == r['expected_root'] == s['binding_compute_root'], 'binding compute root')
        heaps = r['heaps']
        require(len(heaps) <= 2 and len(set(heaps)) == len(heaps) and all(uint(h,positive=True) for h in heaps),'binding heaps')
        review_root(r['compute'],heaps);review_root(r['graphics'],heaps)
    return dict(native_binding_snapshots_verified=8,native_bindings_unchanged=True,native_binding_restoration_tested=False)


def review_projection(rows):
    header,prepared,g,j = rows[0],rows[2],rows[13],rows[15]['jitter']
    require([j[k] for k in ('fault','pending','completed','matched','injections','restorations')] == [0,0,8,8,6,6] and j['hooks_disabled'],'projection closure')
    records = j['records'];require(len(records) == 8,'projection count')
    require(1 <= j['selection_attempts'] <= 20 and j['selection_skipped'] == j['selection_attempts']-1,'projection selection')
    require(j['selected_frame'] == records[0]['frame']+1 == records[1]['frame'] and j['selected_candidate'] == records[1]['candidate'],'projection preview')
    strong = floats([j['selected_candidate'] & 0xffffffff,j['selected_candidate'] >> 32],2)
    require(all(.3125 <= abs(v) <= .5 for v in strong),'first stimulus strength')
    require(len({r['candidate'] for r in records[1:-1]}) == 6,'distinct stimuli')
    result=[]
    for i,(r,sample,gpu) in enumerate(zip(records,rows[4:12],g['records'])):
        q=sample['before']
        require(r['stage'] == i and all(r[k] for k in ('valid','returned','acknowledged','restored','projection_valid','matrix_relation','provider_stable','matrices_valid')),'projection validity')
        require(all(r[k] == q[k] for k in ('frame','epoch','bank','context','view','renderer')) and r['async'] == sample['projection_async'],'projection/request identity')
        require(r['caller'] == header['base']+0x228e834 and r['context'] == r['renderer']+0x710,'projection call site')
        require(r['camera_type'] in (header['base']+0x350d188,header['base']+0x33f82a8) and all(uint(r[k],positive=True) for k in ('camera','table','constants','thread')),'projection pointers')
        require(r['provider_use'] in (0,1) and not r['provider_sizing']&1 and r['wrapper'] == 0 and r['backend'] == prepared['backend'] and r['device'] == prepared['device'],'projection provider')
        require(records[0]['enter_tick'] <= r['enter_tick'] <= r['exit_tick'] <= sample['tick'] < records[0]['enter_tick']+2000,'projection before scheduling')
        require(r['injected'] is (0<i<7) and r['restored_bits'] == 0,'jitter scope')
        if i:
            require(r['frame'] == records[i-1]['frame']+1 and r['epoch'] == records[i-1]['epoch']+1,'projection adjacency')
            require(all(r[k] == records[0][k] for k in ('camera','camera_type','table','view','async','viewport_bits','buffer_id','projection_id','unjittered_id','matrix_ids','words')),'projection sequence identity')
        pair=[r['candidate'] & 0xffffffff,r['candidate'] >> 32]
        values=floats(pair,2);require(any(values) and all(abs(v)<=.5 for v in values),'jitter bounds')
        require(r['table20_bits'] == pair and r['jitter_bits'] == (pair if 0<i<7 else [0,0]),'selected jitter')
        require(gpu['projection_matched'] and gpu['projection_frame'] == r['frame'] == gpu['frame'] and gpu['projection_epoch'] == r['epoch'] and gpu['jitter_bits'] == r['jitter_bits'],'projection/GPU receipt')
        require(floats(r['viewport_bits'],2) == (2560,1440),'projection dimensions')
        verified=matrices(r)
        projection,plain=floats(r['projection_bits'],16),floats(r['unjittered_bits'],16)
        jx,jy=floats(r['jitter_bits'],2)
        for row in range(4):
            for col in range(4):
                at=4*row+col;delta=plain[4*row+3]*(jx/2560 if col==0 else jy/1440 if col==1 else 0)
                require(abs(projection[at]-plain[at]-delta)<=8*2**-23*max(1e-6,abs(projection[at]),abs(plain[at]),abs(delta)),'projection jitter relation')
        result.append(dict(frame=r['frame'],jitter=[jx,jy],predicted_raster_shift=[jx/2,-jy/2],**verified))
    return dict(projection_request_association_verified=True,projection_gpu_association_verified=True,
                jitter_scopes_restored=6,projection_samples=result,color_jitter_verified=False,independent_dlaa_verified=False)


def review(raw,dll_sha,directory):
    result=deferred_review(raw,dll_sha,projection=True)
    rows=parse(raw,projection=True)
    result.update(review_trace(rows[13],rows[12]['requests'],read_points=True))
    result.update(review_bindings(rows[13]))
    result.update(review_projection(rows))
    if rows[0]['version']=='0.6.1':result.update(review_shutdown(rows[13],rows[-1]['first_disable_tick']))
    result.update(review_images(rows[14],directory))
    return result


def review_shutdown(g,first_disable):
    s=g['submission'];requested=g['collection_close_requested'];closed=g['collection_close_tick'];cutoff=s['stopped_tick']
    require(g['collection_closed'] is True and g['completed_before_close'] is True,'completion before cutoff')
    require(s['started_tick'] <= requested <= cutoff <= closed <= first_disable <= g['first_disable_tick'],'collector closes before hook removal')
    require(0 < g['journal_events_at_close'] == len(s['events']) <= 4096,'journal frozen before unhook')
    for r in s['samples']:
        require(s['started_tick'] <= r['completed_tick'] <= requested and r['completed_value']==1 and r['completed'],'fences sampled before cutoff')
        retire=[e for e in s['events'] if e['enter']==r['retire'] and e['kind']==0]
        require(len(retire)==1 and retire[0]['exit_tick']<=requested,'retirement before cutoff')
    require(all(e['enter_tick']<=cutoff and e['exit_tick']<=closed for e in s['events']),'only admitted events drain across cutoff')
    return dict(completion_before_hook_removal_verified=True,journal_events_at_close=g['journal_events_at_close'])


if __name__ == '__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',type=Path,required=True);ap.add_argument('--dll-sha256',required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();require(not a.output.exists() and a.log.stat().st_size <= 4194304,'artifact bounds')
    result=review(a.log.read_bytes(),a.dll_sha256,a.log.resolve().parent)
    with a.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2))

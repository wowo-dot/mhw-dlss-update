"""Verify one owned depth readback, without claiming scene stage or jitter proof."""
import array
import hashlib
import math
import struct
import sys
from projection_provenance import require, uint
from projection_submission import recording, batch

LIMIT = 64*1024*1024

def load(path, record, control=None):
    require(control is None or type(control) is int and control in (0,2,3,4), 'control stage')
    suffix='-depth.f32' if control is None else f'-depth-control-{control}.f32'
    require(record.get('file') == path.stem+suffix, 'depth filename')
    f = path.with_name(record['file'])
    require(0 < f.stat().st_size <= LIMIT, 'depth file bound')
    return f.read_bytes()

def validate(t, review, record, raw, same_frame=False, control=False):
    require(record.get('saved') is True and type(raw) is bytes and 0 < len(raw) <= LIMIT and
            hashlib.sha256(raw).hexdigest() == record.get('sha256'), 'depth bytes/hash')
    d = record.get('details', {})
    require(d.get('schema') == 1 and d.get('phase') == 7 and d.get('copy_attempted') is True and
            d.get('copy_returned') is True and d.get('resources_retained') is True and d.get('mapped') is True and
            d.get('retention') == 'process-lifetime-after-issue' and d.get('recording_retired') is True, 'depth completion/retention')
    w, h = d.get('width'), d.get('height')
    require(uint(w,32) and uint(h,32) and 0 < w <= 8192 and 0 < h <= 8192 and w*h*4 == len(raw), 'depth extent')
    footprint = d.get('footprint')
    require(type(footprint) is list and len(footprint)==6 and all(uint(v) for v in footprint) and
            footprint[:5] == [0,39,w,h,1] and footprint[5]>=w*4 and footprint[5]%256==0, 'depth footprint')
    require(d.get('format')==39 and d.get('subresource')==0 and d.get('xyz')==[0,0,0] and d.get('has_box') is False and
            d.get('rows')==h and d.get('row_bytes')==w*4 and d.get('pixel_bytes')==len(raw) and
            d.get('allocation_bytes')==footprint[5]*(h-1)+w*4 <= LIMIT, 'depth full copy layout')
    require(d.get('buffer_heap')==3 and d.get('buffer_state')==0x400 and d.get('fence_flags')==0 and
            d.get('queue_signal_attempts')==1 and d.get('fence_value')==d.get('completed_value')==1 and
            uint(d.get('signal_result'),32) and d['signal_result']<0x80000000 and
            uint(d.get('map_result'),32) and d['map_result']<0x80000000, 'private fence/map contract')
    require(all(uint(d.get(k)) and d[k]>0 for k in ('buffer','fence','completion_tick')), 'private object identity')
    require(d['buffer'] not in {r['object'] for r in t['copy_resources']} and
            d['fence'] not in {f['object'] for f in t['fences']}, 'private object observed as native')
    require(review.get('view_descriptor_provenance_verified') and review.get('selected_recording_submitted'), 'depth view/submission qualification')
    s = d.get('site', {})
    require(all(uint(s.get(k)) and s[k]>0 for k in ('reset','barrier','copy','copy_exit','begin','end','close','execute',
            'execute_exit','signal_begin','signal_end','resource','identity','command','allocator','queue')), 'depth site fields')
    m = t['marker']
    require(s.get('request_control',False) is control and
            uint(s.get('marker')) and s['marker']==(0 if control else m['order']) and
            (s['resource'],s['identity']) == (m['resource'],m['identity']), 'depth marker identity')
    copies = [c for c in review.get('depth_copy_calls',[]) if c['order']==s['copy']]
    require(len(copies)==1,'readback native copy missing')
    c = copies[0]
    require(c['recording_submitted'] and c['full_depth_source'] and not c['reasons'] and c['kind']==16 and
            c['command']==s['command'] and c['queue']==s['queue'] and c['submission']==s['execute'] and
            c['reset']==s['reset'] and c['close']==s['close'] and c['local_copy_source_state']==0x800, 'readback generation/state')
    require(c['source']['object']==s['resource'] and c['source']['identity']==s['identity'] and
            c['source']['desc'][2:8]==[w,h,1,1,39,1] and c['source']['desc'][8:]==[0,0,2] and
            c['destination']['desc'][2:8]==[w,h,1,1,41,1] and c['destination']['desc'][8:]==[0,0,1], 'readback resource descriptions')
    execute = next((e for e in t['history'] if e['kind']==10 and e['enter']==s['execute']), None)
    require(execute is not None and execute['exit']==s['execute_exit'] and execute['object']==s['queue'], 'readback Execute')
    retire=next((e for e in t['history'] if e['kind']==8 and e['enter']==d.get('retire_reset')),None)
    require(retire is not None and retire['exit']==d.get('retire_reset_exit') and retire['object']==s['command'] and
            retire['result']<0x80000000 and execute['exit']<retire['enter']<retire['exit'], 'readback recording retirement')
    require(next(q['desc'][0] for q in t['queues'] if q['object']==s['queue'])==0, 'readback queue type')
    reset, close, calls = recording(t,s['command'],execute)
    native = next(e for e in calls if e['enter']==s['copy'])
    barrier = next((e for e in calls if e['enter']==s['barrier'] and e['kind']==9), None)
    require(barrier is not None and calls[calls.index(native)-1] is barrier and reset['resource']==s['allocator'] and
            native['exit']==s['copy_exit'], 'readback immediate barrier/allocator')
    relevant = [b for b in t['barriers'][barrier['first']:barrier['first']+barrier['count']] if b[2] in (s['resource'],native['backing'])]
    require([0,0,s['resource'],0,0xffffffff,0x10,0x800] in relevant and
            [0,0,native['backing'],0,0xffffffff,0xc0,0x400] in relevant and len(relevant)==2, 'readback stage declaration')
    require((same_frame or s['marker']<native['enter']) and reset['exit']<barrier['enter']<barrier['exit']<native['enter']<native['exit']<
            s['begin']<s['end']<close['enter']<close['exit']<execute['enter']<execute['exit']<s['signal_begin']<s['signal_end'] and
            d['completion_tick']>=execute['tick'], 'readback chronology')
    require(all(s['end']<e['enter'] for e in calls[calls.index(native)+1:]), 'readback overlaps native list call')
    for other in t['history']:
        if other['kind']==10 and other is not execute and s['command'] in batch(t,other) and other['enter']>reset['enter']:
            later,_,_ = recording(t,s['command'],other)
            require(later['enter']>execute['exit'], 'readback recording resubmitted')
    values = array.array('f'); values.frombytes(raw)
    if sys.byteorder != 'little': values.byteswap()
    finite = [v for v in values if math.isfinite(v)]
    stats = dict(finite=len(finite),nonfinite=len(values)-len(finite),zero=values.count(0),one=values.count(1),
                 outside_unit=sum(v<0 or v>1 for v in finite),minimum=min(finite,default=0),maximum=max(finite,default=0))
    logged = d.get('pixels', {})
    require(all(logged.get(k)==v for k,v in stats.items() if k not in ('minimum','maximum')), 'depth pixel counters')
    for k in ('minimum','maximum'):
        require(type(logged.get(k)) in (int,float) and math.isfinite(logged[k]) and
                struct.pack('<f',logged[k])==struct.pack('<f',stats[k]), 'depth pixel extrema')
    require(stats['nonfinite']==0 and stats['outside_unit']==0, 'invalid depth pixel values')
    return dict(depth_readback_completed=True,depth_pixels_observed=True,depth_pixel_count=len(values),depth_pixel_statistics=stats,
                depth_has_variation=stats['minimum']<stats['maximum'],depth_readback_copy_order=s['copy'],
                depth_readback_submission=s['execute'],depth_readback_file_sha256=record['sha256'],
                depth_readback_frame_relation='later CPU copy of selected resource; exact producer frame not established',
                final_scene_depth_verified=False,raster_jitter_verified=False)

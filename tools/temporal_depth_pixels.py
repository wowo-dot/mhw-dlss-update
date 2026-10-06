"""Verify matched native-copy depth readbacks; pixel alignment remains separate."""
from temporal_limits import frame_count
from temporal_scaled_geometry import active_rectangle,depth_operation,depth_draw_states
import hashlib
from pathlib import Path
from temporal_shader import require
from temporal_consumer import derive_states

def review_depth_pixels(header,capture,folder):
    n=frame_count(header)
    t=capture['depth_readback'];trace=capture['submission'];routes=capture['depth_routes']
    require(t['enabled'] is True and t['retention']=='process-lifetime-after-issue' and trace['depths_enabled'] is True,'depth readback mode')
    copies=t['copies'];require(len(copies)==n and folder is not None,'depth copy count/artifact directory');folder=Path(folder)
    events=trace['events'];consumers=routes['consumers']
    selected=[r for r in consumers if r['pixel_receipt']!=0xffffffff]
    require(len(selected)==n and {r['pixel_receipt'] for r in selected}==set(range(n)),'depth native copy selection')
    local_orders={v[k] for v in routes['requests'] for k in ('enter','exit')}
    local_orders.update(v['order'] for v in routes['selections'])
    local_orders.update(v[k] for v in consumers for k in ('enter','exit','copy','copy_exit'))
    local_orders.update(b[k] for v in consumers for b in v['barrier_calls'] for k in ('enter','exit'))
    orders={e[k] for e in events for k in ('enter','exit')}
    orders.update(s[k] for s in trace['samples'] for k in ('signal','signal_exit','completed_order','dispatch_forward','copy_begin','copy_end','color_begin','color_end'))
    result=[]
    for r in consumers:
        if r['pixel_receipt']==0xffffffff:require(r['pixel_begin']==r['pixel_end']==0,'unselected depth commands')
    for i,(p,cpu,s) in enumerate(zip(copies,capture['records'],trace['samples'])):
        r=next(v for v in selected if v['pixel_receipt']==i);q=routes['requests'][r['receipt']];marker=events[r['marker_index']];d=cpu['scene_depth']
        draw=depth_operation(header,r,q)
        if draw:
            selection=routes['selections'][i]
            eligible=[(j,x) for j,x in enumerate(routes['requests']) if
                all(x[k]==selection[k] for k in ('context','view','container','frame','epoch','bank','async')) and
                x['destination']['owner']==selection['owner'] and x['destination']['resource']==selection['resource'] and x['exit']<selection['order']]
            require(eligible and max(eligible,key=lambda item:item[1]['exit'])[0]==r['receipt'],'depth draw predecessor')
        require(p['phase']==4 and all(p[k] is True for k in ('attempted','transitioned','copied','restored','retained','mapped')) and p['map_result']==0,'depth copy closure')
        require(p['receipt']==i and p['frame']==cpu['frame']==q['frame'] and p['packet']==cpu['packet'],'depth pixel frame association')
        require(p['command']==s['command']==r['command']==marker['object'] and p['allocator']==s['allocator'] and p['reset']==s['reset'],'depth exact TAA generation')
        require(p['resource']==s['depth_resource']==q['destination']['resource']==d['resource'] and p['identity']==s['depth_identity']==r['destination']['identity']==d['identity'],'depth pixel resource')
        require(p['marker']==s['depth_marker']==marker['enter'] and marker['kind']==4 and p['begin']==s['depth_begin'] and p['end']==s['depth_end'] and s['depth_ok'] is True,'depth copy witnesses')
        require(p['reset']<p['marker']<p['begin']<p['end']<marker['exit']<s['dispatch']<s['copy_begin'],'depth copy journal order')
        require(r['copy_exit']<r['pixel_begin']<r['pixel_end']<r['exit'],'depth copy native order')
        require(not {r['pixel_begin'],r['pixel_end']}&local_orders,'depth native order collision');local_orders.update((r['pixel_begin'],r['pixel_end']))
        require(not {p['begin'],p['end']}&orders,'depth journal order collision');orders.update((p['begin'],p['end']))
        require(all(b['exit']<r['pixel_begin'] or b['enter']>r['pixel_end'] for b in r['barrier_calls']),'depth readback overlaps native barrier')
        states=derive_states([q['source']['resource'],q['destination']['resource']],r['barrier_calls'],r['barriers'],r['copy'])
        legal=depth_draw_states(header,states) if draw else states==[dict(known=True,conflict=False,value=0x800),dict(known=True,conflict=False,value=0x400)]
        require(legal,'depth copy-point states')
        w,h=p['width'],p['height'];require(type(w) is int and type(h) is int and 0<w<=8192 and 0<h<=8192 and w*h*4<=16777216,'depth extent')
        require((w,h)==(d['width'],d['height'])==(capture['width'],capture['height']) and cpu['rectangle']==active_rectangle(header,cpu,w,h),'depth dimensions')
        require([p[k] for k in ('format','source_flags','subresource','state_before','copy_state','state_after')]==[41,1,0,4 if draw else 1024,2048,4 if draw else 1024],'depth restored state')
        pitch=(w*4+255)&~255;allocation=pitch*(h-1)+w*4
        require(p['footprint']==[0,41,w,h,1,pitch] and p['row_bytes']==w*4 and p['rows']==h and p['allocation_bytes']==allocation<=16777216,'depth footprint')
        require(p['pixel_bytes']==w*h*4,'depth pixel size')
        name=f"{header['motion_prefix']}.depth-{i}.f32";require(p['file']==name and Path(name).name==name,'depth artifact name')
        path=folder/name;require(path.stat().st_size==p['pixel_bytes'],'depth artifact size')
        sha=hashlib.sha256(path.read_bytes()).hexdigest();require(sha==p['sha256'],'depth artifact hash')
        result.append(dict(frame=p['frame'],file=name,sha256=sha,width=w,height=h,format='R32_FLOAT',bytes=p['pixel_bytes'],copy_receipt=r['receipt']))
    all_copies=copies+capture['motion_readback']['copies']+capture['color_readback']['copies']
    buffers=[p['readback_buffer'] for p in all_copies]
    require(all(type(v) is int and v>0 for v in buffers) and len(set(buffers))==3*n,'depth paired allocation identities')
    require(not set(buffers)&{p['resource'] for p in all_copies},'depth paired source alias')
    require(sum(p['allocation_bytes'] for p in all_copies)<=header['total_readback_limit'],'depth total allocation budget')
    return dict(scene_depth_readback_verified=True,scene_depth_pixels_saved=True,scene_depth_readbacks=result,
                depth_color_motion_triplets_verified=True,scene_depth_content_alignment_verified=False,
                scene_depth_copy_ready=False,scene_depth_final_writer_verified=False,typed_views_verified=False,dlaa_ready=False)

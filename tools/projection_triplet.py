"""Qualify request-stamped controls around one traced stimulus draw."""
from projection_provenance import require, uint
from projection_contract import floats
import projection_alignment as alignment
import projection_frames as frames
import projection_readback as readback


def validate(records, t, review, loader, count=3):
    from review_projection_binding import one
    require(type(count) is int and count in (3,5),'depth sample count')
    stages=[i for i in range(count) if i!=1]
    require(loader is not None, 'missing control pixel loader')
    require(review.get('stimulus_cpu_sequence_verified') is True and
            review.get('selected_projection_copy_frame_matched') is True and
            review.get('selected_draw_precedes_depth_copy_in_queue_order') is True,
            'triplet stimulus qualification')
    sources=[c['source'] for c in one(records,'stimulus')['calls']]
    require(len(sources)==count,'CPU source count')
    controls=[r for r in records if r['event']=='depth_control']
    require(len(controls)==count-1 and [r.get('stage') for r in controls]==stages and
            all(type(r['stage']) is int for r in controls), 'control record stages')
    selections=t['frame_packets'].get('controls',[])
    require(len(selections)==count-1 and [s.get('stage') for s in selections]==stages, 'control selection stages')
    main=one(records,'depth_readback');results={1:review};depths={1:main['details']}
    native_orders={e[k] for e in t['watch']+t['history'] for k in ('enter','exit')}
    marker=t['marker'];order_set=set(native_orders)|{marker['order'],t['draw_return'],marker['bind_enter'],marker['bind_exit']}
    for r,choice in zip(controls,selections):
        stage=r['stage'];d=r['details'];s=choice.get('selection',{})
        # Validate the unmodified journal, not a fabricated marker for controls.
        summary=readback.validate(t,review,r,loader(stage,r),same_frame=True,control=True)
        summary.update(frames.validate(t,d,sources[stage]))
        alignment.selection(t,sources[stage],d,s)
        depths[stage]=d;results[stage]=summary
        require(s['order'] not in order_set,'control selection order collision');order_set.add(s['order'])
        for key in ('width','height','format','subresource'):
            require(d[key]==depths[1][key], 'triplet extent/format mismatch')
        require(d['site']['queue']==depths[1]['site']['queue'], 'triplet queue mismatch')
    selected=t['frame_packets']['selection']
    require(selected['order'] not in order_set,'stimulus selection order collision')
    chosen=[selections[0]['selection'],selected,*[s['selection'] for s in selections[1:]]]
    require(all(a['order']<b['order'] for a,b in zip(chosen,chosen[1:])), 'triplet selection chronology')
    copies=[next(c for c in review['depth_copy_calls'] if c['order']==depths[i]['site']['copy']) for i in range(count)]
    positions=[(c['submission'],c['batch_index'],c['order']) for c in copies]
    require(all(a<b for a,b in zip(positions,positions[1:])), 'triplet GPU submission sequence')
    queue=depths[1]['site']['queue']
    executes=sorted((e for e in t['history'] if e['kind']==10 and e['object']==queue and
                     positions[0][0]<=e['enter']<=positions[-1][0]),key=lambda e:e['enter'])
    require(all(a['exit']<b['enter'] for a,b in zip(executes,executes[1:])), 'triplet overlapping queue submissions')
    require(len({d['buffer'] for d in depths.values()})==len({d['fence'] for d in depths.values()})==count,
            'triplet private allocations reused')
    # A reused descriptor address is not lifetime continuity. Require one
    # construction covering every selected native copy and the stimulus draw.
    start=min(c['order'] for c in copies);end=max(c['order'] for c in copies)
    ctors=[e for e in t['watch'] if e['kind']==0 and e['object']==marker['view'] and e['exit']<marker['order']]
    require(ctors,'control descriptor constructor missing')
    ctor=max(ctors,key=lambda e:e['enter'])
    require(ctor['exit']<start and ctor['resource']==marker['resource'] and ctor['handle']==marker['handle'],
            'control descriptor lifetime begins after baseline')
    p=one(records,'provenance_ready')
    for e in t['watch']:
        if not ctor['exit']<e['enter']<=end:continue
        require(not(e['kind'] in (0,1) and e['object']==marker['view']), 'control view reconstructed/destroyed')
        require(not(e['kind']==2 and e['handle']==marker['handle']), 'control DSV overwritten')
        if e['kind'] in (3,4):
            require(not any(a<=marker['handle']<a+n*p['stride'] for a,n in e['ranges']), 'control DSV copied over')
    # Added GPU commands consume distinct journal orders and cannot masquerade
    # as native calls or other slots' commands.
    order_set.update(selected[k] for k in ('order',))
    order_set.update(e[k] for e in t['frame_packets']['receipts']+t['frame_packets']['consumers'] for k in ('enter','exit'))
    for d in depths.values():
        for key in ('begin','end','signal_begin','signal_end'):
            value=d['site'][key]
            require(uint(value) and value not in order_set, 'triplet added-command order collision')
            order_set.add(value)
    jitter=floats(sources[1]['jitter_bits'],2)
    # The native projection adds (jx/width,jy/height) in NDC. D3D's viewport
    # halves X and flips/halves Y when mapping NDC to resource pixels.
    result=dict(depth_triplet_request_stamps_verified=True,depth_triplet_readbacks_completed=True,
        depth_triplet_frames=[s['frame'] for s in sources],depth_triplet_epochs=[s['epoch'] for s in sources],
        depth_triplet_copy_positions=[list(p) for p in positions],
        depth_control_results=[dict(stage=i,**results[i]) for i in stages],
        expected_stimulus_pixel_displacement=[jitter[0]/2,-jitter[1]/2],
        depth_control_draw_bindings_verified=False,depth_pixel_producer_frame_verified=False,
        raster_jitter_verified=False,
        depth_triplet_scope='Three original request stamps and completed copies of the same depth; only the stimulus has a selected draw binding. Pixel displacement remains to be measured.')
    if count==5:
        for key in list(result):
            if key.startswith('depth_triplet_'):result[key.replace('depth_triplet_','depth_sequence_')]=result.pop(key)
        result['depth_sequence_scope']='Five original request stamps and completed copies of the same depth; only stage 1 has a selected draw binding. Pixel displacement remains to be measured.'
        result['expected_sequence_pixel_displacements']=[[xy[0]/2,-xy[1]/2] for xy in [floats(s['jitter_bits'],2) for s in sources[1:-1]]]
    return result

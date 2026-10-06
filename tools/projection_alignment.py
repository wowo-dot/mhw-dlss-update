"""Match the selected projection request to a later submitted copy stage."""
from projection_provenance import require, uint
from projection_submission import recording, batch


def ordered_path(t, review, copy):
    m=t['marker'];q=copy['queue']
    require(review['submission_queue']==q,'projection/copy queue mismatch')
    start=(review['submission_order'],review['submission_batch_index'],m['order'])
    end=(copy['submission'],copy['batch_index'],copy['order'])
    require(start<end,'copy does not follow selected draw in submission order')
    submits=sorted((e for e in t['history'] if e['kind']==10 and e['object']==q and start[0]<=e['enter']<=end[0]),key=lambda e:e['enter'])
    require(submits and submits[0]['enter']==start[0] and submits[-1]['enter']==end[0],'alignment submissions missing')
    require(all(a['exit']<b['enter'] for a,b in zip(submits,submits[1:])), 'alignment queue submission overlap')
    checked=[]
    for submit in submits:
        for index,command in enumerate(batch(t,submit)):
            position=(submit['enter'],index)
            if position<start[:2] or position>end[:2]:continue
            reset,close,calls=recording(t,command,submit)
            if position==start[:2]:
                require(command==m['command'] and reset['exit']<m['order']<t['draw_return']<close['enter'], 'selected draw recording interval')
            if position==end[:2]:require(command==copy['command'],'copy recording position')
            for call in calls:
                if not start<(submit['enter'],index,call['enter'])<end:continue
                require(call['kind']!=7,'unobserved bundle between draw and copy')
                require(not(call['kind']==6 and call['handle']==m['handle']),'depth clear between draw and copy')
                require(not(call['kind'] in (15,16) and call['backing']==m['resource']),'depth overwrite between draw and copy')
                if call['kind']==9:
                    for b in t['barriers'][call['first']:call['first']+call['count']]:
                        require(not(b[0]==1 and (not b[2] or not b[3] or m['resource'] in (b[2],b[3]))),'depth alias between draw and copy')
            checked.append(dict(submission=submit['enter'],index=index,command=command,reset=reset['enter'],close=close['enter']))
    return checked


def selection(t, source, depth, s):
    f=t['frame_packets']
    require(s.get('published')==2 and all(uint(s.get(k)) and s[k]>0 for k in
      ('order','core_order','renderer','context','constants','thread')) and uint(s.get('buffer_id'),32), 'projection selection publication')
    for name,original in [('core_order','order'),('renderer','renderer'),('context','context'),('constants','constants'),
                          ('thread','thread'),('buffer_id','buffer_id'),('frame','frame'),('epoch','epoch'),('bank','bank'),('async_mode','async_mode')]:
        require(uint(s.get(name)) and s[name]==source[original], 'projection selection '+name)
    require(depth['site'].get('selection')==s['order'], 'readback selection order')
    native_orders={e[k] for e in t['history']+t['watch'] for k in ('enter','exit')}
    receipt_orders={e[k] for e in f['receipts']+f['consumers'] for k in ('enter','exit')}
    require(s['order'] not in native_orders|receipt_orders|{t['marker']['order'],t['draw_return']},'selection order collision')
    consumer=next(c for c in f['consumers'] if c['copy']==depth['site']['copy'])
    producer=f['receipts'][consumer['receipt']-1]
    require(s['order']<producer['enter'],'copy packet predates selected projection')
    require(all(producer[k]==s[k] for k in ('context','thread','frame','epoch','bank','async_mode')), 'projection/copy request frame mismatch')


def validate(t, review, source, depth):
    selection(t,source,depth,t['frame_packets'].get('selection',{}))
    copy=next(c for c in review['depth_copy_calls'] if c['order']==depth['site']['copy'])
    path=ordered_path(t,review,copy)
    return dict(selected_projection_copy_frame_matched=True,selected_draw_precedes_depth_copy_in_queue_order=True,
      depth_copy_recorded_before_selected_draw=copy['order']<t['marker']['order'],
      depth_alignment_recordings=path,intervening_depth_clear_observed=False,
      depth_readback_frame_relation='matching original request stamp; selected draw precedes copy in queue order',
      depth_pixel_producer_frame_verified=False,raster_jitter_verified=False)

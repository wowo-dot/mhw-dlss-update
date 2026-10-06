"""Independently replay TAA list generations and private fence witnesses."""
from temporal_shader import require


def review_submission(capture, *, sample_count=3):
    require(sample_count in (3,9),'submission sample limit')
    trace=capture['submission']
    require(trace['started_tick']==capture['started_tick'],'submission clock')
    require(trace['fault']==0 and trace['pending']==0 and trace['hooks_disabled'] is True,'submission closure')
    require(trace['retained'] is False and trace['pins']==trace['released'],'submission references')
    registries={}
    for key,limit in (('lists',128),('queues',8),('allocators',128)):
        rows=trace[key];require(0<len(rows)<=limit,'submission registry bounds')
        require(all(type(r[k]) is int and 0<r[k]<2**64 for r in rows for k in ('object','identity')),'submission identities')
        require(len({r['object'] for r in rows})==len(rows)==len({r['identity'] for r in rows}),'submission identity reuse')
        registries[key]={r['object']:r['identity'] for r in rows}
    require(trace['pins']>=sample_count+sum(len(v) for v in registries.values()),'submission pin count')
    events=trace['events'];require(0<len(events)<=4096,'submission event budget')
    by_order={};orders=set();previous=0;submitted=0
    for e in events:
        require(type(e['kind']) is int and e['kind'] in range(5 if 'depth_routes' in capture else 4),'submission event kind')
        require(type(e['enter']) is int and type(e['exit']) is int and previous<e['enter']<e['exit'],'submission event order')
        previous=e['enter']
        require(trace['started_tick']<=e['enter_tick']<=e['exit_tick']<trace['started_tick']+2000,'submission event interval')
        require(not {e['enter'],e['exit']}&orders,'duplicate event order');orders.update((e['enter'],e['exit']))
        require(e['returned'] is True and e['valid'] is True and 0<=e['result']<0x80000000,'submission event result')
        require(type(e['thread']) is int and e['thread']>0,'submission event thread')
        by_order[e['enter']]=e
        if e['kind']==2:
            require(e['object'] in registries['queues'] and e['allocator']==0,'execution queue')
            batch=e['lists'];require(0<len(batch)<=128 and all(type(p) is int and 0<p<2**64 for p in batch),'execution batch')
            submitted+=len(batch)
        else:
            require(e['object'] in registries['lists'] and e['lists']==[],'list event identity')
            require(e['allocator'] in registries['allocators'] if e['kind']==0 else e['allocator']==0,'allocator event identity')
    require(submitted<=8192,'submission pointer budget')
    samples=trace['samples'];require(len(samples)==sample_count,'submission sample count')
    require(len({s['fence'] for s in samples})==sample_count and all(type(s['fence']) is int and 0<s['fence']<2**64 for s in samples),'private fence identities')
    require(len({s['dispatch'] for s in samples})==sample_count,'dispatch witness reuse')
    require({e['enter'] for e in events if e['kind']==3}=={s['dispatch'] for s in samples},'dispatch witness count')
    result=[]
    for i,(s,native,cpu) in enumerate(zip(samples,capture['native_compute']['records'],capture['records'])):
        require(s['receipt']==i and s['frame']==native['frame']==cpu['frame'] and s['packet']==native['packet']==cpu['packet'],'submission request association')
        require(s['command']==native['command'] and registries['lists'].get(s['command'])==native['identity'],'submission list association')
        require(cpu['enter_tick']>=trace['started_tick']+150,'submission warmup')
        require(s['allocator'] in registries['allocators'] and s['queue'] in registries['queues'],'submission owned identities')
        witness=[]
        for key,kind in (('reset',0),('dispatch',3),('close',1),('execute',2),('retire',0)):
            require(s[key] in by_order,'missing submission witness')
            event=by_order[s[key]];require(event['kind']==kind,'wrong submission witness')
            require(event['object']==(s['queue'] if kind==2 else s['command']),'submission witness identity')
            witness.append(event)
        reset,dispatch,close,execute,retire=witness
        require(reset['allocator']==s['allocator'],'submission allocator association')
        require(reset['exit']<dispatch['enter']<dispatch['exit']<close['enter']<close['exit']<execute['enter']<execute['exit']<retire['enter'],'submission generation order')
        require(s['dispatch_exit']==dispatch['exit'] and s['execute_exit']==execute['exit'] and s['retire_exit']==retire['exit'],'submission exit witnesses')
        require(dispatch['thread']==native['thread'],'native dispatch thread association')
        require(execute['lists'].count(s['command'])==1,'submission list multiplicity')
        # Replay every operation touching this list, including overlapping or
        # duplicate execution. A pointer match alone cannot prove a generation.
        relevant=[]
        for e in events:
            touches=e['object']==s['command'] if e['kind']!=2 else s['command'] in e['lists']
            if touches and e['exit']>reset['enter'] and e['enter']<retire['exit']:
                relevant.append(e)
        relevant.sort(key=lambda e:e['enter'])
        require(all(a['exit']<b['enter'] for a,b in zip(relevant,relevant[1:])),'overlapping list operations')
        require([e['enter'] for e in relevant if e['kind']==0]==[reset['enter'],retire['enter']],'intervening reset')
        require([e['enter'] for e in relevant if e['kind']==1]==[close['enter']],'intervening close')
        require([e['enter'] for e in relevant if e['kind']==2]==[execute['enter']],'resubmission before retirement')
        require(s['signal_attempts']==1 and s['signal_result']==0 and s['signaled'] is True,'private queue signal')
        require(s['completed'] is True and s['completed_value']==1,'private fence completion')
        require(execute['exit_tick']<=s['completed_tick']<trace['started_tick']+2000,'private fence deadline')
        require(execute['exit']<s['signal']<s['signal_exit']<s['completed_order'],'private fence order')
        extra={s['signal'],s['signal_exit'],s['completed_order']}
        require(len(extra)==3 and not extra&orders,'private fence order collision');orders.update(extra)
        result.append(dict(frame=s['frame'],reset=s['reset'],dispatch=s['dispatch'],execute=s['execute'],
                           completion=s['completed_order'],retire=s['retire'],queue=s['queue']))
    return dict(gpu_submission_verified=True,gpu_completion_verified=True,recording_retirement_verified=True,
                submitted_frames=result,readback_eligible=False,motion_pixels_saved=False)

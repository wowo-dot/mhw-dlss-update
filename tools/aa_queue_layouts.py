"""Review the native base/haze owner graph independently of runtime booleans."""
from arm_aa_stream import require


def review_return(s, base, count):
    require(s['reason']==0 and s['callbacks']==1 and all(s[k] is True for k in
        ('inserted','removed','returned','complete','callback_valid','neutral','published')), 'layout completion')
    before=s['before']
    require(count in (4,5) and before['count']==count and len(before['nodes'])==count, 'native layout size')
    require(before['enabled']==before['collecting']==1 and before['taa']==before['fxaa']==0 and
            before['rectangle']==[0,0,2560,1440], 'native AA-Off layout')
    owners=[before['attachments'][0],*before['scratch'],before['attachments'][1]]
    require(len(owners)==4 and len(set(owners))==4 and all(type(x) is int and x>0 for x in owners), 'native owners')
    specification={0:(0x3538400,[0,0,0],owners[0],owners[1]),
                   100:(0x3537a70,[0,0,0],owners[1],owners[2]),
                   1024:(0x3536c98,[0,1,0],0,owners[2]),
                   134217713:(0,[1,0,0],owners[1] if count==5 else owners[2],owners[3])}
    if count==5:specification[20000]=(0x35372b0,[0,0,0],owners[2],owners[1])
    require({n['order'] for n in before['nodes']}==set(specification) and
            len({n['descriptor'] for n in before['nodes']})==count,'native node set')
    assigned=[]
    for n in before['nodes']:
        vt,flags,src,dst=specification[n['order']]
        require(n['descriptor']>0 and n['phase']==15 and n['vtable']==(base+vt if vt else 0) and
                n['flags']==flags and (n['unit']>0)==bool(vt),'native node identity')
        assigned.append(dict(n,input=src,output=dst))
    require(s['assigned']==dict(before,nodes=assigned),'predicted native endpoints')
    require(all(type(s[k]) is int and s[k]>0 for k in ('descriptor','unit','vtable')) and
            s['descriptor'] not in {n['descriptor'] for n in assigned} and s['color_owner']==owners[1], 'owned identity')
    inserted=dict(descriptor=s['descriptor'],unit=s['unit'],vtable=s['vtable'],phase=15,order=2,input=0,output=owners[1],flags=[0,1,0])
    expected=sorted(assigned+[inserted],key=lambda n:n['order'])
    for key in ('callback','after','restored'):
        q=s[key]
        for identity in ('manager','context','view','renderer','frame','epoch','bank','rectangle','taa','fxaa','scratch','attachments','enabled'):
            require(q[identity]==before[identity], 'layout identity: '+identity)
        require(q['collecting']==0 and q['packets']>=before['packets'],'layout phase')
        require(q['count']==(count if key=='restored' else count+1) and
                q['nodes']==(assigned if key=='restored' else expected),'native endpoint preservation: '+key)
    require(s['after']['packets']>=s['callback']['packets']+1 and
            s['restored']['packets']==s['after']['packets'],'layout publication and restoration')


def review_layouts(c, base):
    s=c['scheduler_layouts'];require(s['policy']=='base-or-haze-v1','layout policy')
    counters=('base_admissions','haze_admissions','base_returns','haze_returns','base_evaluations','haze_evaluations','haze_entries','haze_exits')
    require(all(type(s[k]) is int and s[k]>=0 for k in counters),'layout counters')
    for prefix in ('base','haze'):
        require(s[prefix+'_admissions']==s[prefix+'_returns']==s[prefix+'_evaluations'],'layout frame accounting')
    require(s['base_evaluations']+s['haze_evaluations']==c['frames'] and
            s['first_failed_return'] is None,'layout completion accounting')
    if s['haze_evaluations']:
        review_return(s['first_haze'],base,5)
    else:
        require(s['first_haze'] is None,'no unsupported haze receipt')
    require(s['haze_entries']<=s['haze_returns'] and s['haze_exits']<=s['base_returns'] and
            abs(s['haze_entries']-s['haze_exits'])<=1,'layout transitions')
    if s['haze_exits']:
        require(s['haze_evaluations']>0,'haze exit has prior evaluations')
        review_return(s['first_base_after_haze'],base,4)
    else:
        require(s['first_base_after_haze'] is None,'no unsupported base-return receipt')
    return dict(haze_frames=s['haze_evaluations'],haze_layout_verified=s['haze_evaluations']>0,
                haze_entries=s['haze_entries'],haze_exits=s['haze_exits'])

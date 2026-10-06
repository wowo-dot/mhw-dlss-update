"""Bounded output-readiness observations; no new resource admission proof."""
from arm_continuous_dlaa import require

def number(x):
    return type(x) is int and 0 <= x < 2**64-1

def review(s, previous=None):
    route=s['scaled_route'];t=route['readiness']
    require(t['schema'] in (1,2,3,4) and t['policy']==f"bounded-output-readiness-v{t['schema']}" and t['diagnostic_only'] is True and t['capacity']==8,'readiness policy')
    require(all(number(t[k]) for k in ('attempts','accepted','rejected')),'readiness counts')
    prepared=t['schema']>=3
    stages=12 if t['schema']==4 else 4 if t['schema']>=2 else 3;mask=(1<<stages)-1
    executed_mask=4095 if route.get('overlays',{}).get('policy') in ('native-in-place-extent-v1','native-extent-and-dependencies-v2') else mask
    reasons=t['reasons']
    require(len(reasons)==13 and all(number(x) for x in reasons),'readiness reasons')
    require(sum(reasons)==t['attempts']==t['accepted']+t['rejected'] and reasons[0]==t['accepted'],'readiness coverage')
    if prepared:
        require(route['schema']==3 and s['frames']+route['spatial_frames']<=t['accepted']<=s['frames']+route['spatial_frames']+1,'prepared readiness agreement')
    else:require(t['accepted']==route['claimed'],'readiness route agreement')
    require(t['rejected']==s['scaled_depth_capture']['readiness_rejected'],'readiness rejection agreement')
    def sample(v, baseline=False):
        require(number(v['ordinal']) and 0<v['ordinal']<=t['attempts'] and number(v['rejection']) and v['rejection']<=t['rejected'],'readiness sample identity')
        reason=v['reason'];require(type(reason) is int and 0<=reason<13,'readiness sample reason')
        require((reason==0 and v['rejection']==0) if baseline else (reason>0 and 0<v['rejection']<=v['ordinal']),'readiness sample channel')
        for key,n in (('at',6),('request',4),('plan',12),('active',9)):
            require(len(v[key])==n and all(number(x) for x in v[key]),'readiness sample '+key)
        p=v['plan'];a=v['active']
        require(p[0]<32 and a[0]<32 and all(x in (0,1) for x in p[3:6]) and p[6]<=executed_mask and all(x in (0,1) for x in a[4:6]) and a[6]<=stages,'readiness bounded state')
        require(type(v['accepting']) is bool and type(v['stamp_equal']) is bool and number(v['fault']),'readiness flags')
        require(all(number(v[k]) and v[k]<=mask for k in ('pipeline_missing_mask','pipeline_signature_mask')),'readiness pipeline masks')
        stage=v['stage'];require(type(stage) is int and 0<=stage<=stages,'readiness stage')
        require((stage<stages) if reason in (9,10) else (stage==stages),'readiness stage relevance')
        if reason==0:
            require(p[4]==1 and a[1]==v['pipeline_missing_mask']==v['pipeline_signature_mask']==0 and v['stamp_equal'] and v['at'][4]>0 and v['at'][5]>0,'readiness admitted state')
            require(p[5]==1 if prepared else p[5]==p[6]==0,'readiness prepared/native state')
        if reason==1:require(v['accepting'] is False,'readiness inactive receipt')
        if reason==2:require(v['fault']>0,'readiness fault receipt')
        if reason==3:require(a[1]>0,'readiness active receipt')
        if reason==4:require(v['at'][4]==0 or v['at'][5]==0,'readiness argument receipt')
        if reason==5:require(p[4]==0 or (prepared and p[5]==0),'readiness unbuilt receipt')
        if reason==6:require(p[5]==1,'readiness claimed receipt')
        if reason==7:require(p[6]>0,'readiness executed receipt')
        if reason==8:require(not v['stamp_equal'],'readiness stamp receipt')
        if reason==9:require(v['pipeline_missing_mask']&(1<<stage),'readiness missing pipeline')
        if reason==10:require(not(v['pipeline_missing_mask']&(1<<stage)) and v['pipeline_signature_mask']&(1<<stage),'readiness pipeline signature')
    if t['accepted']:sample(t['baseline'],True)
    else:require(t['baseline'] is None,'readiness empty baseline')
    samples=t['samples'];ordinals=list(range(1,min(8,t['rejected'])+1))+([t['rejected']] if t['rejected']>8 else [])
    require(len(samples)==len(ordinals) and [v['rejection'] for v in samples]==ordinals,'readiness bounded samples')
    seen=[0]*13;last=0
    for v in samples:
        sample(v);require(v['ordinal']>last,'readiness receipt order');last=v['ordinal'];seen[v['reason']]+=1
    require(all(seen[i]<=reasons[i] for i in range(13)),'readiness retained reason coverage')
    if previous:
        require(all(t[k]>=previous[k] for k in ('attempts','accepted','rejected')) and all(x>=y for x,y in zip(reasons,previous['reasons'])),'readiness counter rollback')
        if previous['baseline'] is not None:require(t['baseline']==previous['baseline'],'readiness baseline changed')
        for v in previous['samples'][:8]:require(samples[v['rejection']-1]==v,'readiness initial evidence changed')
    return t

"""Replay bounded state-rejection evidence without changing state admission."""
from arm_continuous_dlaa import require
from copy import deepcopy
from continuous_native_states import review as review_native
MAX32=2**32-1


def uint(value, label, limit=2**64-1):
    require(type(value) is int and 0<=value<=limit,label)


def proof(rows, role, expected):
    known=False;value=transitions=0;index=MAX32;bit=1<<role
    for i,kind,flags,sub,before,after,a,b in rows:
        if kind==1 and ((a|b)&(bit|8)):return [2,value,transitions,i]
        if kind!=0 or not a&bit:continue
        transitions+=1;index=i
        if flags:return [3,value,transitions,index]
        if sub!=MAX32:return [4,value,transitions,index]
        if known and value!=before:return [5,value,transitions,index]
        known=True;value=after
    return [1 if not known else 0 if value==expected else 6,value,transitions,index]


def review(status, previous=None, *, evidence_only=False):
    t=status['state_trace']
    require(t.get('schema') in (1,2) and t.get('policy')=='consumer-local-state-v1','state trace policy')
    require(t.get('first_capacity')==8 and t.get('barrier_capacity')==16,'state trace bounds')
    for k in ('total','output_rejections','temporal_rejections','omitted'):uint(t[k],k)
    require(t['total']==t['output_rejections']+t['temporal_rejections'],'state evidence partition')
    if not evidence_only:require(t['total']==status['dispatch_reasons'][6],'state rejection partition')
    require(t['omitted']==max(0,t['total']-9),'state trace omission coverage')
    if not evidence_only:
        r=status['resolve']
        require(t['output_rejections']<=r['unclaimed_native']+r['pending'],'state native-forward coverage')
        require(t['temporal_rejections']<=r['unfiltered']+r['pending'],'state unfiltered coverage')
    samples=t['samples'];require(type(samples) is list,'state samples')
    ordinals=list(range(1,min(8,t['total'])+1))+([t['total']] if t['total']>8 else [])
    require([s['ordinal'] for s in samples]==ordinals,'state sample ordinal coverage')
    replayed=0
    for s in samples:
        if t['schema']==2:review_native(s['provenance'])
        for k in ('ordinal','tick','frame','epoch','sequence','generation','list','packet','barrier_count','relevant_barriers','native_nodes','native_relevant'):uint(s[k],k)
        require(all(s[k]>0 for k in ('ordinal','frame','sequence','generation','list','packet')),'state sample identity')
        require(type(s['temporal']) is bool and type(s['native_complete']) is bool,'state sample flags')
        resources=s['resources'];require(type(resources) is list and len(resources)==3,'state roles')
        for resource in resources:
            for k in ('object','backing','resource','width','height','format'):uint(resource[k],k);require(resource[k]>0,'state resource identity')
            p=resource['proof'];require(type(p) is list and len(p)==4,'state proof')
            for n in p:uint(n,'state proof word',MAX32)
            require(p[0]<=6 and p[2]<=128 and (p[3]==MAX32 or p[3]<s['barrier_count']),'state proof ranges')
        require((resources[0]['proof'][0]==resources[1]['proof'][0]==0 and resources[2]['proof'][0]!=0) if s['temporal'] else
                (resources[0]['proof'][0]!=0 or resources[1]['proof'][0]!=0),'state rejection stage')
        rows=s['barriers'];require(type(rows) is list and len(rows)==min(s['relevant_barriers'],16),'state barrier cap')
        require(s['relevant_barriers']<=s['barrier_count']<=128,'state barrier bounds')
        prior_index=-1
        for row in rows:
            require(type(row) is list and len(row)==8,'state barrier row')
            for n in row:uint(n,'state barrier word',MAX32)
            i,kind,flags,sub,before,after,a,b=row
            require(prior_index<i<s['barrier_count'] and kind<=2 and a<=15 and b<=15 and (a|b),'state barrier metadata')
            prior_index=i
        if len(rows)==s['relevant_barriers']:
            require([proof(rows,i,want) for i,want in enumerate((0x40,8,0xc0))]==[v['proof'] for v in resources],'state replay differs')
            replayed+=1
        native=s['native'];require(type(native) is list and len(native)==min(s['native_relevant'],16),'native state cap')
        require(s['native_relevant']<=s['native_nodes']<=128,'native state bounds')
        prior_index=-1
        for row in native:
            require(type(row) is list and len(row)==4,'native state row')
            for n in row:uint(n,'native state word',MAX32)
            i,mask,before,after=row
            require(prior_index<i<s['native_nodes'] and 0<mask<8 and before<=65535 and after<=65535,'native state metadata');prior_index=i
    if previous:
        require(all(t[k]>=previous[k] for k in ('total','output_rejections','temporal_rejections','omitted')),'state counter rollback')
        n=min(8,previous['total'])
        require(samples[:n]==previous['samples'][:n],'first state samples changed')
    return dict(deepcopy(t),state_trace_verified=True,replayed_samples=replayed,diagnostic_only=True)

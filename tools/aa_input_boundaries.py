"""Check bounded input-barrier receipts without trusting runtime verdicts."""
from arm_aa_stream import require


def review_boundary(s):
    require(s['schema']==1 and s['native_nodes'] in (4,5), 'boundary schema/layout')
    resources=s['resources'];batch=s['batch']
    require(len(resources)==len(set(resources))==3 and all(type(r) is int and r>0 for r in resources), 'boundary inputs')
    require(s['count']==len(batch) and 0<len(batch)<=1024 and s['end_order']==s['begin_order']+len(batch)
            and 0<=s['marker_order']<=s['begin_order'], 'boundary ordering')
    require(s['list']>0 and s['generation']>0 and s['frame']>0, 'boundary ownership')
    for key in ('marker','before','after'):
        require(len(s[key])==3,'input state count')
        for i,entry in enumerate(s[key]):
            require(type(entry['tracked']) is bool and (entry['resource']==resources[i] if entry['tracked'] else entry['resource']==0), 'tracked input identity')
            require(entry['state']['last']<=(s['marker_order'] if key=='marker' else s['begin_order'] if key=='before' else s['end_order']), 'state order')
    relevant=0;color=motion=False
    for b in batch:
        hit=b['type']==1 or (b['type']==0 and b['resource'] in resources)
        if not hit:continue
        relevant+=1
        if b['type']==0 and b['flags']==0 and b['subresource']==0xffffffff and b['before']==4 and b['after']==128:
            color|=b['resource']==resources[0];motion|=b['resource']==resources[2]
    mask=0
    for i,e in enumerate(s['after']):
        v=e['state']
        if not e['tracked'] or not v['known'] or v['issue']!=0 or v['value']!=128:mask|=1<<i
    accepted=relevant==2 and color and motion and mask==0
    require((s['relevant'],s['color'],s['motion'],s['invalid_states'],s['accepted'])==(relevant,color,motion,mask,accepted), 'boundary verdict matches evidence')
    return dict(accepted=accepted,relevant=relevant,color=color,motion=motion,invalid_states=mask,
                frame=s['frame'],native_nodes=s['native_nodes'])


def review_boundaries(c):
    s=c['input_boundaries'];require(s['policy']=='exact-color-motion-v1' and s['first_rejection'] is None,'no input boundary rejection')
    for name in ('base','haze'):
        if c['scheduler_layouts'][name+'_evaluations']:
            require(review_boundary(s['first_'+name])['accepted'],'successful input boundary')
        else:require(s['first_'+name] is None,'no unsupported input receipt')

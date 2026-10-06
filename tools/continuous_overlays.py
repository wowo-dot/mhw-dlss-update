"""Recording observations and bounded in-place overlay accounting."""
from copy import deepcopy
from arm_continuous_dlaa import require

def review(route, previous=None, final=False):
    t=route['overlays'];states=route['recording_states']
    number=lambda x:type(x) is int and 0<=x<2**64-1
    require(t['schema']==1 and t['policy'] in ('native-in-place-extent-v1','native-extent-and-dependencies-v2'),'overlay policy')
    if t['policy']=='native-extent-and-dependencies-v2':
        d=route['dependency_route']
        require(d.get('schema') in (1,2) and all(number(d.get(k)) for k in ('copy_draws','radial_draws','rejection')),'dependency route schema')
        require(d['copy_draws']+d['radial_draws']+(d.get('dof_draws',0) if d['schema']==2 else 0)<=t['draws'] and d['rejection']<=4,'dependency draw coverage')
        if d['schema']==2:require(number(d.get('dof_draws')),'DOF draw counter')
        require(type(d.get('last_description')) is list and len(d['last_description'])==6 and all(number(v) for v in d['last_description']),'dependency description')
        if previous and 'dependency_route' in previous:
            require(all(d[k]>=previous['dependency_route'][k] for k in (('copy_draws','radial_draws','dof_draws') if d['schema']==2 else ('copy_draws','radial_draws'))),'dependency rollback')
    require(all(number(t[k]) for k in ('prepared','draws','completed','rejected')),'overlay counters')
    require(t['completed']<=t['draws']<=t['prepared']<=8*route['claimed'],'overlay draw coverage')
    require(t['prepared']-t['completed']<=8*(route['claimed']-route['complete']),'overlay in-flight bound')
    if 'shared_key_prepared' in t or 'shared_key_completed' in t:
        require(all(number(t[k]) for k in ('shared_key_prepared','shared_key_completed')),'shared-key counters')
        a,b=t['shared_key_prepared'],t['shared_key_completed']
        require(b<=a<=route['claimed'] and b<=route['complete'] and 2*a<=t['prepared'] and 2*b<=t['completed'],'shared-key frame coverage')
        require(a-b<=route['claimed']-route['complete'],'shared-key in-flight bound')
        if previous:
            require(all(t[k]>=previous['overlays'].get(k,0) for k in ('shared_key_prepared','shared_key_completed')),'shared-key rollback')
        if final:require(a==b,'shared-key cleanup coverage')
    if 'extended_draws' in t:
        require(number(t['extended_draws']) and t['extended_draws']<=t['draws'],'extended overlay draw coverage')
        layout=t['extended_layout']
        if t['extended_draws']:
            require(type(layout) is list and len(layout)==6 and all(number(v) for v in layout),'extended overlay layout')
            stride,count,pf,po,uf,uo=layout;sizes={2:16,6:12,16:8}
            require(16<stride<=255 and 2<=count<=32 and pf in sizes and uf in sizes,'extended overlay formats')
            require(po%4==uo%4==0 and po+sizes[pf]<=stride and uo+sizes[uf]<=stride and
                    (po+8<=uo or uo+8<=po),'extended overlay attribute bounds')
        else:require(layout is None,'empty extended overlay layout')
        if previous:
            require(t['extended_draws']>=previous['overlays'].get('extended_draws',0),'extended overlay rollback')
    require(states['schema']==1 and states['policy']=='native-recording-observation-v1','recording state policy')
    require(all(number(states[k]) for k in ('observations','outside_selected_callbacks','imported')),'recording state counters')
    require(states['outside_selected_callbacks']<=states['observations'],'unscoped observation coverage')
    if previous:
        for channel,keys in ((t,('prepared','draws','completed','rejected')),(states,('observations','outside_selected_callbacks','imported'))):
            old=previous['overlays' if channel is t else 'recording_states']
            require(all(channel[k]>=old[k] for k in keys),'overlay/state counter rollback')
    if final:require(t['prepared']==t['draws']==t['completed'],'overlay cleanup coverage')
    result=dict(overlays=t,recording_states=states)
    if 'dependency_route' in route:result['dependency_route']=route['dependency_route']
    return deepcopy(result)

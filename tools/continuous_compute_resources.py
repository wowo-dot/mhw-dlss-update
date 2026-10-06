"""Validate bounded GPU-description and recording-state observations, not pixels."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import integer, vector


def review(t, cpu, previous=None, final=False):
    require((t['schema'],t['policy']) in ((1,'native-compute-resource-observation-v1'),(2,'native-compute-entry-observation-v2'),(3,'native-compute-stable-recipe-v3')), 'compute resource policy')
    require(t['diagnostic_only'] is True and t['gpu_work_added'] is False
            and t['pixel_footprint_verified'] is False and t['descriptor_contents_verified'] is False, 'compute observation scope')
    require(integer(t['lost']) and type(t['stopped']) is bool and (not t['lost'] or t['stopped']), 'compute trace bounds')
    samples=t['samples']
    require(type(samples) is list and len(samples)==len(cpu['samples'])<=2, 'compute resource capture association')
    for s,c in zip(samples,cpu['samples']):
        require(s['at']==c['at'] and integer(s['error'],2), 'compute sample identity')
        resources=s['resources']
        require(type(resources) is list and len(resources)<=16, 'compute resource capacity')
        ids=[]
        for r in resources:
            vector(r['owner'],7);vector(r['description'],12);require(type(r['held']) is bool, 'resource ownership')
            ids.append(r['owner'][3]);require(ids[-1]>0, 'resource identity')
            if not s['error']:
                d=r['description']
                require(d[0]==3 and 1<=d[2]<=16384 and 1<=d[3]<=16384 and d[4]==1
                        and 1<=d[5]<=16 and d[7:9]==[1,0] and d[11]>0, 'actual texture description')
                require(t['stopped'] or r['held'], 'resource reference retained while observing')
            if final:require(not r['held'], 'diagnostic resource reference released')
        require(len(ids)==len(set(ids)), 'unique captured resources')
        require(len(s['passes'])==6, 'six compute observations')
        for p,d in zip(s['passes'],c['computes']):
            require(p['packet']==d['packet'] and integer(p['error'],6) and integer(p['calls']), 'compute observation identity')
            require(type(p['entered']) is bool and type(p['returned']) is bool, 'compute return flags')
            vector(p['at'],5);vector(p['pipeline'],2)
            require(type(p['uses']) is list and len(p['uses'])<=16 and len(p['states'])==len(p['uses']), 'compute use coverage')
            expected=[w for w in d['words'] if w&15 in (2,3)]
            if not s['error']:require([v[0] for v in p['uses']]==expected, 'all texture bindings covered')
            for use,state in zip(p['uses'],p['states']):
                vector(use,2);vector(state,2,2**32-1)
                require(use[0] in expected and use[1]<len(resources) and state[0] in (0,1)
                        and (state[0] or state[1]==0), 'recorded use/state')
                texture=next(x for x in d['textures'] if x['index']==(use[0]>>4&255))
                require(texture['fields'][3]==ids[use[1]], 'recorded resource binding identity')
            if p['entered']:require(all(p['at'][:4]), 'native entry scope')
            if p['returned']:require(p['entered'], 'return has entry')
            if p['calls'] and not p['error']:
                require(p['calls']==1 and all(p['pipeline']) and p['at'][4]>=p['at'][0], 'native dispatch scope')
            if t['schema']>=2:
                e=p['entry'];q=p['recipe'];prefix=p['observed_prefix']
                vector(e,9);vector(q,2);vector(prefix,12,2**32-1)
                require(e[0]<=13 and all(x in (0,1) for x in e[4:7]) and q[0] in (0,1) and q[1]<=255, 'entry evidence bounds')
                require(q[0] or (q[1]==0 and prefix==[0]*12), 'unreadable recipe has no comparison')
                if q[0]:require(bool(q[1]&2)==(prefix!=d['prefix']), 'recipe prefix difference')
                stable=q[1]
                if t['schema']==3:
                    stable=p['stable_difference']
                    require(integer(stable,255), 'stable recipe mask bounds')
                    expected_mask=0
                    if q[0]:
                        changed=any(a!=b for i,(a,b) in enumerate(zip(prefix,d['prefix'])) if i not in (2,3))
                        expected_mask=(q[1]&~2)|(2 if changed else 0)
                    require(stable==expected_mask, 'stable recipe excludes only state-list pointer')
                if p['error']==1:require(1<=e[0]<=11 and not p['entered'] and not p['calls'], 'entry rejection reason')
                elif p['error']==6:require(e[0] in (12,13), 'entry frame rejection reason')
                else:require(e[0]==0, 'entry reason without entry rejection')
                if e[0]==6:require(not e[4], 'observer absent')
                if e[0]==7:require(e[4] and not e[5], 'observer unknown')
                if e[0]==8:require(e[4] and e[5] and not e[6], 'observer invalid')
                if e[0]==9:require(all(e[4:7]) and not e[7], 'generation missing')
                if e[0]>=10 and e[0]<=11:require(all(e[1:8]), 'recipe checked after valid entry context')
                if e[0]==10:require(q==[0,0], 'recipe unreadable reason')
                if e[0]==11:require(q[0] and stable, 'recipe changed reason')
                if p['entered']:
                    require(all(e[1:9]) and e[2:4]==[p['at'][3],p['at'][1]] and e[7]==p['at'][2], 'entry identity preserved')
                    if p['error']!=3:require(q[0] and not stable, 'entry recipe unchanged')
                if p['error']==3:require(not q[0] or stable, 'API recipe rejection evidence')
    require(type(t['barriers']) is list and len(t['barriers'])<=256, 'barrier capacity')
    known_ids={r['owner'][3] for s in samples for r in s['resources']}
    for e in t['barriers']:
        vector(e,9)
        require(all(e[:3]) and e[2] in known_ids and e[3] in (0,1) and e[4]<=2
                and all(v<=2**32-1 for v in e[5:8]) and e[8] in (0,3), 'native barrier observation')
    if final:require(t['stopped'], 'compute observation stopped')
    if previous:
        require(t['lost']>=previous['lost'] and t['barriers'][:len(previous['barriers'])]==previous['barriers'], 'immutable barrier prefix')
        require(len(samples)>=len(previous['samples']), 'compute sample rollback')
        for s,old in zip(samples,previous['samples']):
            require(s['at']==old['at'] and len(s['resources'])==len(old['resources']), 'resource sample stable')
            for r,prior in zip(s['resources'],old['resources']):
                require(r['owner']==prior['owner'] and r['description']==prior['description'], 'resource description immutable')
            for p,prior in zip(s['passes'],old['passes']):
                require(p['uses']==prior['uses'] and p['calls']>=prior['calls'], 'compute evidence monotonic')
                if prior['returned']:require(p==prior, 'completed dispatch immutable')
    return deepcopy(t)

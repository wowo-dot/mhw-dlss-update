"""Review bounded CPU dependency snapshots; they do not authorize GPU work."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import integer, vector
from read_scaled_haze import program

def review(t, previous=None):
    require(t['schema']==1 and t['policy']=='prepared-draw-dependencies-v1' and t['diagnostic_only'] is True
            and t['capacity']==2 and t['draw_capacity']==4 and t['attempt_limit']==16,'dependency policy')
    require(integer(t['attempts'],16) and type(t['samples']) is list and len(t['samples'])<=2,'dependency bounds')
    vector(t['errors'],6,16)
    require(t['errors'][0]==0 and sum(t['errors'])+len(t['samples'])==t['attempts'],'dependency attempt accounting')
    frames=[]
    for s in t['samples']:
        vector(s['at'],8);frames.append(s['at'][1])
        require(s['at'][1]>0 and s['at'][2]>0 and s['at'][3]>0 and s['at'][6]<=1 and s['at'][7]<=1,'dependency frame identity')
        require(integer(s['key'],2**32-1) and s['key']>>27==15 and integer(s['queue']) and s['queue']>0
                and integer(s['destination']) and s['destination']>0 and integer(s['total'],65534) and s['total']>0,'dependency queue identity')
        require(s['double_snapshot_equal'] is True and s['gpu_verified'] is False,'dependency evidence scope')
        if 'connection' in s:
            vector(s['connection'],3)
            require(s['connection'][0]>0 and s['connection'][2]==s['destination'],'dependency connection identity')
        require(type(s['draws']) is list and 1<=len(s['draws'])<=4,'dependency draw capacity')
        indices=[]
        for d in s['draws']:
            require(integer(d['packet']) and d['packet']>0 and integer(d['index'],s['total']-1),'dependency packet')
            indices.append(d['index']);vector(d['prefix'],18,2**32-1);p=d['prefix']
            ptr=lambda i:p[i]|p[i+1]<<32
            require(p[0]&31==0 and 1<=p[16]<=4 and p[17]<=1048576,'dependency draw arguments')
            vector(d['target'],8);target=d['target']
            require(target[0]==ptr(4) and all(target[i]>0 for i in (0,3,4,5)) and target[1]<=2**32-1
                    and target[6]<=2**32-1 and target[7]<=255,'dependency target')
            require(type(d['rectangle']) is list and len(d['rectangle'])==4
                    and all(type(v) is int and -2**31<=v<2**31 for v in d['rectangle']),'dependency rectangle')
            require(integer(d['shader_set']) and d['shader_set']>0,'dependency shader set')
            vector(d['vertex'],4);ia,stride,offset,arena=d['vertex']
            require(ia==ptr(6) and 1<=stride<=255 and offset<=16777216 and arena>0
                    and offset+(p[17]+p[16])*stride<=33554432,'dependency vertices bound')
            vector(d['vertices'],stride*p[16],255)
            require(type(d['layout']) is list and len(d['layout'])<=32,'dependency layout bound')
            for e in d['layout']:
                require(type(e['semantic']) is str and len(e['semantic'])<64 and all(32<=ord(c)<=126 for c in e['semantic']),'dependency semantic')
                vector(e['fields'],6,2**32-1)
            require(type(d['owners']) is list and len(d['owners'])<=64,'dependency owner count')
            for o in d['owners']:vector(o,3);require(o[0]<=2**32-1 and o[1]<=255,'dependency owner metadata')
            require(type(d['ranges']) is list and len(d['ranges'])==2,'dependency stage ranges')
            used=set()
            for r in d['ranges']:
                vector(r['range'],2,65535);count,start=r['range']
                require(count<=128 and count+start<=4096,'dependency range bounds');vector(r['words'],count,2**32-1)
                for w in r['words']:
                    kind,index=w&15,(w>>4)&255
                    require(kind<=3 and (kind==0 or index<len(d['owners'])),'dependency binding index')
                    if kind in (2,3):used.add(index)
            require(type(d['textures']) is list and len(d['textures'])==len(used),'dependency texture coverage')
            require([x['index'] for x in d['textures']]==sorted(used),'dependency unique texture indices')
            for x in d['textures']:
                vector(x['fields'],7);f=x['fields']
                require(f[0]==d['owners'][x['index']][2] and f[0]>0 and (f[1]>0 or f[2]>0) and f[3]>0
                        and all(v<=2**32-1 for v in f[4:]),'dependency texture identity')
            require(type(d['shaders']) is list and len(d['shaders'])==2,'dependency shader stages')
            for stage,h in enumerate(d['shaders']):
                vector(h['identity'],3);require(all(h['identity']) and integer(h['size'],16384) and h['size']>=52,'dependency shader identity')
                require(type(h['hex']) is str and len(h['hex'])==h['size']*2,'dependency shader byte count')
                program(bytes.fromhex(h['hex']),1-stage)
        require(all(a<b for a,b in zip(indices,indices[1:])),'dependency native queue order')
    require(all(b-a>=60 for a,b in zip(frames,frames[1:])),'dependency sample interval')
    if previous:
        require(t['attempts']>=previous['attempts'] and all(a<=b for a,b in zip(previous['errors'],t['errors']))
                and t['samples'][:len(previous['samples'])]==previous['samples'],'dependency immutable evidence')
    return deepcopy(t)


def review_intermediate(t, previous=None):
    result=review(t,previous)
    for s in t['samples']:
        require('connection' in s and s['connection'][1]>0 and
                s['connection'][1]!=s['destination'],'intermediate color connection')
    return result

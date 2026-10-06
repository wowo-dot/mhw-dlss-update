"""Review complete CPU compute/draw bundles; no GPU execution is inferred."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import integer, vector
from continuous_dependencies import review as review_draws
from read_scaled_haze import program


def constants(d, ranges):
    expected=[]
    for r in ranges:
        for word in r:
            kind,index=word&15,(word>>4)&255
            require(index<len(d['owners']), 'constant owner index')
            if kind:continue
            identity,mode,owner=d['owners'][index]
            size={(11,0):1072,(12,1):112,(501,3):96}.get((word>>12&4095,word>>24))
            require(size is not None and identity==word>>12&4095 and mode==0
                    and owner>0 and owner%16==0 and owner<=2**64-1-size,'typed CPU constant owner')
            if word not in expected:expected.append(word)
    require(len(expected)<=3 and len({w>>24 for w in expected})==len(expected), 'constant register uniqueness')
    require(type(d['constants']) is list and [c['word'] for c in d['constants']]==expected,'constant coverage')
    for c in d['constants']:
        size={(11,0):1072,(12,1):112,(501,3):96}[c['word']>>12&4095,c['word']>>24]
        require(c['size']==size and type(c['hex']) is str and len(c['hex'])==size*2,'constant byte count')
        require(len(bytes.fromhex(c['hex']))==size,'constant hex')


def review(t, previous=None):
    require((t['schema'],t['policy']) in ((2,'prepared-compute-bundle-v1'),(3,'prepared-compute-constants-v1')), 'compute bundle policy')
    base=deepcopy(t);base.update(schema=1,policy='prepared-draw-dependencies-v1')
    review_draws(base)
    for s in t['samples']:
        vector(s['connection'],3)
        require(s['connection'][1]>0 and s['connection'][1]!=s['destination'], 'compute color connection')
        require(type(s['computes']) is list and 1<=len(s['computes'])<=8, 'compute capacity')
        require(type(s['commands']) is list and 2<=len(s['commands'])<=16, 'compute command capacity')
        if t['schema']==3:
            for d in s['draws']:constants(d,[r['words'] for r in d['ranges']])
        for d in s['computes']:
            require(integer(d['packet']) and d['packet']>0 and integer(d['index'],s['total']-1), 'compute packet')
            vector(d['prefix'],12,2**32-1);p=d['prefix']
            require(p[0]&31==6 and (p[4]|p[5]<<32)>0 and all(1<=n<=65535 for n in p[6:9])
                    and p[9]&255==0 and p[10:12]==[0,0], 'direct dispatch arguments')
            require(integer(d['shader_set']) and d['shader_set']>0 and integer(d['technique'],2**32-1), 'compute shader state')
            owners=d['owners'];require(type(owners) is list and 1<=len(owners)<=64, 'compute owners')
            for owner in owners:
                vector(owner,3);require(owner[0]<2**32-1 and owner[1]<=255 and owner[2]>0, 'compute owner identity')
            vector(d['range'],2,65535);count,start=d['range']
            require(1<=count<=128 and count+start<=4096, 'compute binding range')
            vector(d['words'],count,2**32-1)
            require(type(d['views']) is list and len(d['views'])==count, 'compute descriptor coverage')
            used=set()
            for word,view in zip(d['words'],d['views']):
                kind,index=word&15,(word>>4)&255
                require(kind<=3 and (kind==0 or index<len(owners)), 'compute binding index')
                vector(view,2)
                if kind in (2,3):used.add(index);require(all(view), 'compute descriptor identity')
                else:require(view==[0,0], 'non-texture descriptor')
            require(type(d['textures']) is list and [x['index'] for x in d['textures']]==sorted(used), 'compute texture coverage')
            for x in d['textures']:
                vector(x['fields'],7);f=x['fields']
                require(f[0]==owners[x['index']][2] and (f[1] or f[2]) and f[3]>0
                        and all(v<=2**32-1 for v in f[4:]), 'compute texture identity')
            h=d['shader'];vector(h['identity'],3)
            require(all(h['identity']) and integer(h['size'],65536) and h['size']>=52, 'compute shader identity')
            require(type(h['hex']) is str and len(h['hex'])==h['size']*2, 'compute shader byte count')
            program(bytes.fromhex(h['hex']),5)
            if t['schema']==3:constants(d,[d['words']])
        indices=[];addresses=set();seen_draws=[];seen_computes=[]
        for command in s['commands']:
            vector(command,4);index,packet,raw,typed=command;opcode=raw&31
            require(index<s['total'] and packet>0 and packet not in addresses and raw<=2**32-1
                    and opcode in (0,6,11), 'compute command identity')
            indices.append(index);addresses.add(packet)
            if opcode==11:require(typed==2**32-1, 'state packet type');continue
            records=s['computes'] if opcode==6 else s['draws']
            require(typed<len(records), 'typed command index');d=records[typed]
            require([d['index'],d['packet'],d['prefix'][0]]==[index,packet,raw], 'typed command identity')
            (seen_computes if opcode==6 else seen_draws).append(typed)
        require(all(a<b for a,b in zip(indices,indices[1:])) and seen_draws==list(range(len(s['draws'])))
                and seen_computes==list(range(len(s['computes']))), 'complete ordered command coverage')
    if previous:
        require(t['attempts']>=previous['attempts'] and all(a<=b for a,b in zip(previous['errors'],t['errors']))
                and t['samples'][:len(previous['samples'])]==previous['samples'], 'immutable compute evidence')
    return deepcopy(t)

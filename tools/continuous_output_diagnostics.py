"""Validate bounded CPU identity correlations; never infer GPU completion."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import review as review_commands, gate as command_gate, qualification as command_qualification

def review(s, previous=None):
    r=s['scaled_route']; t=r['diagnostics']
    require(t['schema'] in (1,2) and t['policy']==f"bounded-output-correlations-v{t['schema']}" and t['diagnostic_only'] is True and t['capacity']==4 and t['history_capacity']==128, 'output diagnostics policy')
    stages=4 if t['schema']==2 else 3;mask=(1<<stages)-1
    vertex_policy=t.get('vertex_policy','packed16-v1')
    require(vertex_policy in ('packed16-v1','native-overlay-layout-v1'),'vertex decoding policy')
    if 'command_policy' in t:
        require(t['command_policy']=='prepared-command-rejections-v1','command decoding policy')
        review_commands(t['commands'],previous.get('commands') if previous else None)
    if r.get('overlays',{}).get('policy')=='native-in-place-extent-v1':mask=4095
    def number(v):return type(v) is int and 0<=v<2**64-1
    def vector(v,n):require(type(v) is list and len(v)==n and all(number(x) for x in v), 'output diagnostics vector')
    counters=('graph_attempts','graph_accepted','graph_rejected','order_count','batches','submission_count')
    require(all(number(t[k]) for k in counters), 'output diagnostics counters')
    require(t['graph_attempts']==t['graph_accepted']+t['graph_rejected'], 'output graph coverage')
    require(t['order_count']==r['readiness']['reasons'][7], 'output order coverage')
    require(t['submission_count']<=128*t['batches'], 'output submission bound')
    def ordinals(n):return list(range(1,min(n,4)+1))+([n] if n>4 else [])
    def graph(g,baseline=False):
        require(number(g['ordinal']) and g['ordinal']>0, 'output graph ordinal')
        vector(g['at'],8); vector(g['links'],8)
        require(type(g['count']) is int and 0<=g['count']<=16 and type(g['filters']) is list and len(g['filters'])==g['count'], 'output graph bounds')
        for row in g['filters']:vector(row,7)
        require(number(g['reason']) and g['reason']<=18 and number(g['detail']) and g['detail']<=(8 if t['schema']==2 else 6) and number(g['link_reads']) and g['link_reads']<=15, 'output graph result')
        qualification=g.get('qualification',0)
        if g.get('command_gate') is not None:command_gate(g['command_gate'],g['reason'])
        command_qualification(qualification,g['reason'],g['detail'],g.get('command_gate'))
        require(qualification==0 or g['reason']>0, 'output graph qualification')
        if 'vertex_gate' in g:
            gate=g['vertex_gate']
            if qualification&65535==3:
                require(type(gate) is list and len(gate)==10 and all(type(x) is int and 0<=x<=2**64-1 for x in gate),'vertex gate vector')
                packet,ia,context,n,first,read_mask,stride,offset,arena,failed=gate
                valid_stride=(stride>=16 if vertex_policy=='native-overlay-layout-v1' and qualification>>16>=4 else stride==16)
                require(packet>0 and context==g['at'][3]>0 and n==(4 if qualification>>16<2 or qualification>>16>=4 else 3)
                        and first<=1048576 and stride<=255 and offset<=2**32-1, 'vertex gate identity and bounds')
                require(failed in range(1,8) and read_mask=={1:0,2:1,3:1,4:3,5:3,6:7,7:7}[failed], 'vertex gate read coverage')
                require((read_mask&1 or stride==0) and (read_mask&2 or offset==0) and (read_mask&4 or arena==0), 'vertex gate unread values')
                require((failed==1 or ia>0) and (failed<3 or valid_stride) and (failed<5 or offset<=16777216), 'vertex gate short circuit')
                require((failed!=2 or not valid_stride) and (failed!=4 or offset>16777216)
                        and (failed!=6 or arena==0) and (failed!=7 or arena>2**64-1-33554432), 'vertex gate failed predicate')
            else:require(gate is None,'vertex gate outside failed check')
        require((g['reason']==0 and g['detail']==0) if baseline else g['reason']>0, 'output graph acceptance')
        if baseline:require(g['ordinal']<=t['graph_attempts'], 'output graph baseline ordinal')
        if t['schema']==1 and g['reason']==6 and g['detail']==4:
            require(g['link_reads']&3==3, 'output graph predecessor read')
            # A successful input read with a changed link short-circuits the output read.
            if g['link_reads']&4 and g['links'][5]!=g['links'][4]:require(g['link_reads']&8==0, 'output graph short circuit')
    if t['graph_accepted']:graph(t['graph_baseline'],True)
    else:require(t['graph_baseline'] is None,'output graph empty baseline')
    require([g['ordinal'] for g in t['graphs']]==ordinals(t['graph_rejected']), 'output graph sample coverage')
    for g in t['graphs']:graph(g)
    require([o['ordinal'] for o in t['orders']]==ordinals(t['order_count']), 'output order sample coverage')
    for o in t['orders']:
        require(all(number(o[k]) for k in ('frame','epoch','tick','executed')) and 0<o['executed']<=mask, 'output order identity')
        vector(o['packets'],stages)
        vector(o['identity'],8)
        require(len(o['draws'])==stages and len(o['submissions'])==stages+1,'output order bounds')
        for i,d in enumerate(o['draws']):
            vector(d,11)
            require(d[7] in (0,1) and d[8] in (0,1), 'output draw flags')
            if d[0]:require(d[6]==o['packets'][i], 'output draw packet')
            if d[8]:require(d[0]>0 and d[1]>0 and d[2]>0 and d[3]>0 and d[4]>0 and d[5]>0, 'output validated draw identity')
        for i,a in enumerate(o['submissions']):
            vector(a,7)
            if i:require(a[:2]==o['draws'][i-1][1:3], 'output recording identity')
            if a[3]:require(all(a[j]>0 for j in (0,1,2,3,5,6)) and a[3]<=t['batches'] and a[4]<a[5]<=128, 'output submission receipt')
            else:require(a[2:]==[0]*5, 'output unknown submission')
    if previous:
        require(t.get('command_policy')==previous.get('command_policy'),'command policy changed during run')
        require(vertex_policy==previous.get('vertex_policy','packed16-v1'),'vertex policy changed during run')
        require(all(t[k]>=previous[k] for k in counters), 'output diagnostics rollback')
        if previous['graph_baseline'] is not None:require(t['graph_baseline']==previous['graph_baseline'], 'output graph baseline changed')
        for g in previous['graphs'][:4]:require(t['graphs'][g['ordinal']-1]==g, 'output first graph changed')
        current={o['ordinal']:o for o in t['orders']}
        for old in previous['orders']:
            if old['ordinal'] not in current:continue
            new=current[old['ordinal']]
            for k in ('frame','epoch','tick','executed','packets','identity'):require(new[k]==old[k], 'output order immutable identity')
            for a,b in zip(old['submissions'],new['submissions']):
                if a[3]:require(a==b, 'output observed submission changed')
    return deepcopy(t)

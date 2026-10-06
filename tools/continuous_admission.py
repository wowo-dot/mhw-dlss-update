"""Replay native current-use admission evidence; this is not a GPU state query."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_states import review as review_states, uint


def decision(s):
    p=s['provenance'];resources=s['resources']
    if s['temporal'] or resources[0]['proof'][0]>1 or resources[1]['proof'][0]!=1:return 1
    if (not s['packet'] or not s['generation'] or not s['list'] or not s['native_complete'] or
        len(s['native'])!=s['native_relevant'] or len(s['barriers'])!=s['relevant_barriers']):return 2
    if not resources[0]['resource'] or not resources[1]['resource'] or any(resources[0][k]==resources[1][k] for k in ('object','backing','resource')):return 2
    if (not p['readable'] or not p['scan_complete'] or not p['stable'] or p['matches']!=1 or
        p['index']>=p['packets'] or s['epoch']==2**64-1 or p['epoch']!=s['epoch']+1):return 3
    earlier=p['earlier'];begin=max(0,p['index']-32)
    if len(earlier)!=p['index']-begin:return 3
    if any(e['index']!=begin+i or not e['packet'] or not e['complete'] or len(e['nodes'])!=e['relevant'] for i,e in enumerate(earlier)):return 3
    if len({e['packet'] for e in earlier})!=len(earlier):return 3
    def packet(at):return next((e for e in earlier if e['packet']==at),None)
    def node(e,role,span,before,after):
        matches=[n for n in e['nodes'] if n[1]&role]
        return len(matches)==1 and matches[0][1:]==[role,before,after,span]
    for t in p['trackers'][:2]:
        rows=t['spans']
        if (not t['readable'] or not t['stable'] or not t['complete'] or not 0<len(rows)<=16 or
            t['epoch']!=p['epoch'] or t['used_epoch']!=p['epoch'] or t['initialized']!=1 or t['immediate_epoch']):return 4
        if t['first']!=rows[0][0] or t['last']!=rows[-1][0]:return 4
        for i,a in enumerate(rows):
            if (not a[0] or not a[1] or a[2]>=p['packets'] or a[3]!=(rows[i+1][0] if i+1<len(rows) else 0) or
                a[4]!=(rows[i-1][0] if i else 0) or a[5]>65535 or a[6]>65535 or a[7] or
                a[5]!=(rows[i-1][6] if i else t['initial']) or (i and a[2]<=rows[i-1][2])):return 4
            if any(a[0]==b[0] or a[1]==b[1] for b in rows[:i]):return 4
    color,out=p['trackers'][:2]
    uses=[(i,a) for i,a in enumerate(out['spans']) if a[1]==s['packet']]
    if len(uses)!=1:return 5
    index,use=uses[0]
    if use[2]!=p['index'] or use[5:7]!=[0x400,0x400] or index>1:return 5
    native=[n for n in s['native'] if n[1]&2]
    api=[b for b in s['barriers'] if b[6]&2]
    if len(native)!=1 or native[0][1:]!=[2,0x400,0x400]:return 5
    if len(api)!=1 or api[0][1]!=2 or api[0][2] or api[0][6]!=2:return 5
    if index==0:
        if out['initial']!=0x400:return 5
        if any(n[1]&2 for e in earlier for n in e['nodes']):return 5
    else:
        first=out['spans'][0];e=packet(first[1])
        if (out['initial'] or first[5:7]!=[0,0x400] or not e or e['opcode']!=6 or e['index']+2!=p['index'] or
            first[2]!=e['index'] or not node(e,2,first[0],0,0x400)):return 5
        if any(n[1]&2 for q in earlier[e['index']+1-begin:] for n in q['nodes']):return 5
    if resources[0]['proof'][0]==1:
        reads=[a for a in color['spans'] if a[2]==p['index']]
        if len(reads)!=1:return 6
        read=reads[0];e=packet(read[1])
        if not e or e['opcode']!=6 or e['index']+2!=p['index'] or read[5:7]!=[0x100,4] or not node(e,1,read[0],0x100,4):return 6
        if any(n[1]&1 for n in s['native']):return 6
        if any(n[1]&1 for q in earlier[e['index']+1-begin:] for n in q['nodes']):return 6
    return 0


def review(status,previous=None):
    a=status['state_admission']
    require(a['schema']==1 and a['policy']=='native-current-use-v1','state admission policy')
    for k in ('attempts','accepted','rejected'):uint(a[k],k)
    require(a['attempts']==a['accepted']+a['rejected'],'admission partition')
    trace=review_states({'state_trace':a['evidence']},evidence_only=True)
    require(trace['schema']==2 and trace['total']==a['attempts'] and trace['temporal_rejections']==0,'admission evidence coverage')
    require(trace['replayed_samples']==len(trace['samples']),'complete API evidence')
    results=a['results'];require(type(results) is list and len(results)==len(trace['samples']),'admission result coverage')
    accepted=rejected=0
    for r,s in zip(results,trace['samples']):
        uint(r['ordinal'],'admission ordinal');uint(r['reason'],'admission reason',7)
        require(r['ordinal']==s['ordinal'] and type(r['stable']) is bool,'admission result identity')
        proof=decision(s)
        require(r['reason']==(proof if proof else 0 if r['stable'] else 7),'native state decision replay')
        require(not r['stable'] if proof else True,'unattempted stability check')
        if r['reason']:rejected+=1
        else:accepted+=1
    require(accepted<=a['accepted'] and rejected<=a['rejected'],'admission retained counts')
    require(a['accepted']-accepted+a['rejected']-rejected==trace['omitted'],'admission omitted counts')
    if previous:
        require(all(a[k]>=previous[k] for k in ('attempts','accepted','rejected')),'admission counter rollback')
        n=min(8,previous['attempts'])
        require(results[:n]==previous['results'][:n] and trace['samples'][:n]==previous['evidence']['samples'][:n],'admission first samples changed')
    return dict(deepcopy(a),native_contract_telemetry_verified=True,independent_gpu_state_query=False)

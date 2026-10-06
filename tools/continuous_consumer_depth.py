"""Validate ownership/accounting for a fresh consumer-point depth copy."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_depth import count, vector


def review(status, previous=None):
    t = status['consumer_depth']
    branch_policy=t.get('copy_execution_policy')
    require(branch_policy in (None,'native-opcode10-branches-v1'),'copy execution policy')
    def branches(sample):
        if branch_policy is None:return
        paths=vector(sample['copy_paths'],5,'native copy branches')
        require(paths[0]==0 and sum(paths)<=257,'native copy branch bounds')
        if sample['reason']==0:
            require(paths[4]>=1 and sum(paths)<=sample['indices'][1]-sample['indices'][0]+1,'qualified writer executes child')
    require((t['schema'], t['policy']) in ((1, 'scheduled-consumer-depth-copy-v1'), (2, 'scheduled-read-interval-depth-copy-v2'))
            and t['writer_copy_disabled'] is True, 'consumer depth policy')
    state_count = 'hoisted' if t['schema'] == 1 else 'read_state_copies'
    counters = ('attempts', 'accepted', 'rejected', 'copies', state_count)
    for k in counters:
        count(t[k], 'consumer depth ' + k)
    reasons = vector(t['reasons'], 12, 'consumer depth reasons')
    lease = status['depth_eligibility']['lease']
    require(t['attempts'] == lease['checks'] == sum(reasons), 'consumer depth attempt coverage')
    require(t['rejected'] == lease['rejected'] == t['attempts'] - t['accepted'], 'consumer depth rejection coverage')
    require(reasons[0] == t['accepted'] == t['copies'] == status['depth_copies'], 'consumer depth copy coverage')
    require(t[state_count] <= t['copies'] and reasons[11] == 0, 'consumer depth state bounds')
    rows = t['samples']; n = t['attempts']
    ordinals = list(range(1, min(n, 8)+1)) + ([n] if n > 8 else [])
    require(type(rows) is list and [s['ordinal'] for s in rows] == ordinals, 'consumer depth sample bounds')
    observed_reasons = [0]*12; observed_hoisted = observed_render_target = 0
    for s in rows:
        branches(s)
        at = vector(s['at'], 6, 'consumer depth identity')
        require(all(at), 'consumer depth positive identity')
        reason = count(s['reason'], 'consumer depth reason', 11)
        observed_reasons[reason] += 1
        indices = vector(s['indices'], 3, 'consumer depth indices')
        require(all(i <= 2**32-1 for i in indices), 'consumer depth index range')
        count(s['state'], 'consumer depth state'); count(s['nodes'], 'consumer depth nodes', 17)
        require(type(s['stable']) is bool, 'consumer depth stability flag')
        if t['schema'] == 2:
            path = s['transitions']
            require(type(path) is list and len(path) <= 16, 'consumer depth transition bounds')
            for row in path: vector(row, 5, 'consumer depth transition')
        if reason == 0:
            require(s['stable'] and s['state'] in ((4, 128) if t['schema'] == 1 else (4, 64, 128, 192)), 'consumer depth accepted state')
            if t['schema'] == 1:
                require(indices[0] < indices[1] < indices[2] < 32768 and indices[1]-indices[0] <= 256
                        and indices[2]-indices[1] <= 128, 'consumer depth scheduled order')
                require(s['nodes'] == (2 if s['state'] == 128 else 1), 'consumer depth current transition coverage')
            else:
                writer, consumer, reader = indices
                require(writer < consumer < 32768 and writer < reader < 32768 and consumer-writer <= 256, 'consumer depth scheduled order')
                require(len(path) == s['nodes'] and len(path) >= 1, 'consumer depth complete transition coverage')
                require(path[0] == [writer, writer, writer, 12, 256], 'consumer depth writer transition')
                for prev, cur in zip(path, path[1:]):
                    first, last, node, before, after = cur
                    require(prev[1] < node <= first <= last < 32768 and node <= consumer
                            and before == prev[4] and after in (4, 8, 12), 'consumer depth read interval')
                require((len(path) == 1 and consumer < reader) or (len(path) > 1 and path[1][0] == reader), 'consumer depth first read coverage')
                require(s['state'] == (4 if len(path) == 1 else path[-1][4]*16), 'consumer depth derived state')
            if s['state'] != 4: observed_hoisted += 1
            else: observed_render_target += 1
    require(all(a <= b for a, b in zip(observed_reasons, reasons)) and observed_hoisted <= t[state_count]
            and observed_render_target <= t['accepted']-t[state_count], 'consumer depth retained counter coverage')
    if previous is not None:
        require(branch_policy==previous.get('copy_execution_policy'),'copy execution policy changed')
        require(t['schema'] == previous['schema'] and t['policy'] == previous['policy'], 'consumer depth changed policy')
        require(all(t[k] >= previous[k] for k in counters), 'consumer depth rollback')
        require(all(a >= b for a, b in zip(reasons, previous['reasons'])), 'consumer depth reason rollback')
        current = {s['ordinal']: s for s in rows}
        for s in previous['samples']:
            if s['ordinal'] <= 8 or s['ordinal'] == n:
                require(current.get(s['ordinal']) == s, 'consumer depth changed retained evidence')
    if 'failure_diagnostics' in t:
        d=t['failure_diagnostics']
        require(d['schema']==1 and d['capacity']==8 and d['count']==t['rejected'],'depth failure diagnostic accounting')
        samples=d['samples'];expected=min(d['count'],8)+(d['count']>8)
        require(type(samples) is list and len(samples)==expected,'depth failure diagnostic bounds')
        last=0; observed=[0]*12; retained={s['ordinal']:s for s in rows}
        for sample in samples:
            branches(sample)
            ordinal=count(sample['ordinal'],'depth failure ordinal')
            require(last<ordinal<=t['attempts'],'depth failure order');last=ordinal
            reason=count(sample['reason'],'depth failure reason',11)
            require(reason>0,'depth failure receipt must reject')
            observed[reason]+=1
            if ordinal in retained:require(sample==retained[ordinal],'depth failure receipt disagreement')
            detail=vector(sample['detail'],14,'depth failure detail')
            require(detail[0] in (0,10,11,20,21,22,23,24,25,26,27,28,30,31,32,33,40,41,42,43,50),'depth failure site')
            if reason==6:require(detail[0]!=0,'interval failure needs exact site')
            vector(sample['at'],6,'depth failure identity')
        require(all(a<=b for a,b in zip(observed,reasons)),'depth failure reason coverage')
        if previous and 'failure_diagnostics' in previous:
            old=previous['failure_diagnostics'];require(d['count']>=old['count'],'depth failure rollback')
            for i in range(min(old['count'],8)):
                require(samples[i]==old['samples'][i],'depth failure retained evidence changed')
    return deepcopy(t)

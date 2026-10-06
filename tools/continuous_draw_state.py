"""Validate fault receipts; these diagnostics cannot authorize rendering."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_depth import count, vector


def review(status, previous=None):
    t=status['scaled_route']['draw_state_failures']
    require(t['schema']==1 and t['diagnostic_only'] is True,'draw-state schema')
    n=count(t['count'],'draw-state count')
    samples=t['samples']
    require(type(samples) is list and len(samples)==min(n,2),'draw-state receipt bound')
    for s in samples:
        mask=count(s['mask'],'draw-state mask',511)
        a=vector(s['at'],7,'draw-state identity')
        v=vector(s['values'],13,'draw-state values')
        limit=11 if status['scaled_route'].get('overlays',{}).get('policy')=='native-in-place-extent-v1' else 3
        require(a[3]<=limit and all(a[i] for i in (0,1,4,5,6)),'draw-state callback identity')
        require(all(v[i] in (0,1) for i in (0,1,5,6)),'draw-state flags')
        expected=(1 if not v[0] else 0)|(2 if v[0] and not v[1] else 0)|(4 if v[0] and v[1] and v[2]!=4 else 0)
        expected|=(8 if v[3]!=v[4] else 0)|(16 if not v[5] else 0)|(32 if v[5] and not v[6] else 0)
        expected|=(64 if v[5] and v[6] and v[7]!=v[8] else 0)|(128 if v[9]!=v[10] else 0)|(256 if v[11]!=v[12] else 0)
        require(mask!=0 and mask==expected,'draw-state predicate agreement')
    if previous:
        require(n>=previous['count'],'draw-state count rollback')
        if previous['count']:require(samples[0]==previous['samples'][0],'draw-state first receipt changed')
    return deepcopy(t)

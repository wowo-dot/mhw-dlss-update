"""Native six-pass Gallery coverage and restoration receipts."""
from arm_continuous_dlaa import require

COUNTERS=('qualified','warmups','dispatches','adapted','completed','rejected','restored')
def review(t, previous=None, final=False):
    require(t.get('schema')==1 and t.get('policy')=='native-six-compute-preserved-views-v1','native DOF policy')
    require(all(type(t.get(k)) is int and 0<=t[k]<2**64-1 for k in COUNTERS),'native DOF counters')
    p=t.get('passes')
    require(type(p) is list and len(p)==6 and all(type(x) is int and 0<=x<2**64-1 for x in p),'native DOF pass counters')
    require(sum(p)==t['dispatches'] and t['adapted']==p[0]+p[3] and t['restored']==t['adapted'],'native DOF dispatch/restore coverage')
    require(t['completed']<=min(p)<=max(p)<=t['completed']+32 and max(p)<=t['qualified'],'native DOF bounded frames')
    require(type(t.get('reason')) is int and t['reason'] in (*range(12),*range(20,27)) and type(t.get('pass')) is int and 0<=t['pass']<6,'native DOF rejection receipt')
    if previous:
        require(all(t[k]>=previous[k] for k in COUNTERS) and all(a>=b for a,b in zip(p,previous['passes'])),'native DOF counter rollback')
    if final:require(p==[t['completed']]*6,'native DOF complete six-pass frames')
    return dict(t)

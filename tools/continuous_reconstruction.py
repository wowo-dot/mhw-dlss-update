"""Validate unfiltered-copy lifetime and reconstruction suspension receipts.

This verifies the bounded controller, not whole-renderer exclusive AA coverage.
A latched failure is reported explicitly and is never successful DLAA evidence.
"""
from arm_continuous_dlaa import require


def count(value, name):
    require(type(value) is int and 0 <= value < 2**64-1, name)


def review(s, previous=None, final=False):
    r=s['reconstruction'];u=s['unfiltered_lifetime'];p=u['recordings']
    require(r.get('schema')==1 and r.get('policy')=='suspend-or-latch-unfiltered-v1','reconstruction policy')
    for k in ('failure','suspensions','resumptions'):count(r[k],k)
    require(type(r['waiting']) is bool and r['failure'] in (0,14),'reconstruction state')
    require(r['suspensions']==r['resumptions']+int(r['waiting']),'suspension partition')
    require(not r['failure'] or r['waiting'],'failure cannot resume')
    require(s['activation']['failure_latched']==bool(r['failure']),'renderer/reconstruction failure agreement')
    require(u.get('schema')==1,'unfiltered lifetime schema')
    for k in ('signal_value','completed_value'):count(u[k],k)
    require(p.get('schema')==1 and p.get('policy')=='reset-and-fence-v1' and p.get('capacity')==128,'unfiltered retention policy')
    keys=('created','submitted','retired','released','live','peak','resource_references','awaiting_submission','awaiting_reset','awaiting_gpu')
    for k in keys:count(p[k],k)
    require(u['completed_value']<=u['signal_value']<=p['submitted']<=p['created'],'unfiltered completion order')
    require(p['released']<=p['retired']<=p['submitted'],'unfiltered release proofs')
    require(p['live']==p['created']-p['released']<=p['peak']<=128,'bounded unfiltered recordings')
    require(p['resource_references']==4*p['live'],'unfiltered references')
    require(p['awaiting_submission']==p['created']-p['submitted'],'unfiltered pending submissions')
    require(p['awaiting_reset']==p['created']-p['retired']<=p['live'],'unfiltered replayable recordings')
    require(p['live']-p['awaiting_reset']<=p['awaiting_gpu']<=p['submitted']-p['released'],'unfiltered GPU coverage')
    count(s['zero_jitter_projections'],'zero-jitter projections')
    require(s['zero_jitter_projections']<=s['jitter_injections'],'zero-jitter scope coverage')
    reasons=s['dispatch_reasons'];require(type(reasons) is list and len(reasons)==12,'dispatch reason schema')
    for n in reasons:count(n,'dispatch reason')
    require(sum(reasons)<=s['skips'],'dispatch reason coverage')
    if previous:
        old=previous['reconstruction'];prior=previous['unfiltered_lifetime']
        require(not old['failure'] or r['failure']==old['failure'],'failure latch cleared')
        require(all(r[k]>=old[k] for k in ('suspensions','resumptions')),'reconstruction counter rollback')
        require(all(u[k]>=prior[k] for k in ('signal_value','completed_value')),'unfiltered fence rollback')
        require(all(p[k]>=prior['recordings'][k] for k in ('created','submitted','retired','released','peak')),'unfiltered counter rollback')
        require(s['zero_jitter_projections']>=previous['zero_jitter_projections'],'zero-jitter rollback')
        require(all(a>=b for a,b in zip(reasons,previous['dispatch_reasons'])),'dispatch reason rollback')
    if final or s['hooks_disabled']:
        require(u['signal_value']==u['completed_value'] and p['awaiting_submission']==p['awaiting_gpu']==0,'unfiltered GPU drainage')
        require(p['live']==p['awaiting_reset'],'only replayable copies retained at stop')
    return dict(reconstruction=dict(r),unfiltered_lifetime=dict(u),zero_jitter_projections=s['zero_jitter_projections'],
                dispatch_reasons=list(reasons),controller_telemetry_verified=True,feature_failed=bool(r['failure']),exclusive_aa_verified=False)

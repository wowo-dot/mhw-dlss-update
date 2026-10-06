"""Verify scoped renderer activation telemetry, without asserting exclusive AA."""
from arm_continuous_dlaa import require

COUNTERS=('settings_writes','fault','pending','active_context','frames',
          'taa_visits','taa_admitted','taa_rejected','competing_suppressed',
          'jitter_suppressed','table_forced','sharpen_suppressed')
MONOTONIC=tuple(k for k in COUNTERS if k not in ('pending','active_context'))

def review(status,previous=None,final=False):
    m=status['activation']
    v2=m.get('schema')==2
    require((v2 and m.get('policy')=='native-postqueue-preparation-v2') or
            (m.get('schema')==1 and m.get('policy')=='scoped-native-preparation-v1'),'activation policy')
    counters=COUNTERS+('layer_flag_writes' if v2 else 'layer_writes',)
    if v2:counters+=('queue_visits','queue_inserted','queue_existing','queue_rejected','execute_unowned')
    if 'queue_unfiltered' in m:
        counters+=('queue_unfiltered',)
        require(type(m.get('failure_latched')) is bool,'activation failure latch')
        require(m['failure_latched'] or m['queue_unfiltered']==0,'unfiltered queue requires latched failure')
    for k in counters:
        require(type(m.get(k)) is int and 0<=m[k]<2**64-1,'activation counter '+k)
    require(type(m.get('requested')) is bool and type(m.get('hooks_disabled')) is bool,'activation state types')
    require(m['settings_writes']==m['layer_flag_writes' if v2 else 'layer_writes']==0,'activation must not write shared settings/layer flags')
    require(m['fault']==0,'activation fault')
    require(m['taa_visits']==m['taa_admitted']+m['taa_rejected'],'activation admission partition')
    c=m.get('native_choices')
    require(type(c) is list and len(c)==4 and all(type(v) is int and 0<=v<2**64-1 for v in c),'activation choice counters')
    require(sum(c)==m['frames'],'activation frame choice coverage')
    # The runtime snapshots frame/choice and admission partitions under its
    # telemetry lock. Assembly gate counters are independent monotonic totals.
    if v2:
        q=m.get('queue_reasons')
        require(type(q) is list and len(q)==6 and all(type(x) is int and 0<=x<2**64-1 for x in q),'activation queue reasons')
        require(sum(q)==m['queue_rejected'],'activation queue rejection coverage')
        completed=m['queue_inserted']+m['queue_existing']+m['queue_rejected']+m.get('queue_unfiltered',0)
        require(completed<=m['queue_visits'],'activation queue partition')
        if final:
            require(completed==m['queue_visits'],'activation queue closure')
            require(m['queue_inserted']+m['queue_existing']>0,'activation queue exercised')
    if previous:
        require(all(m[k]>=previous[k] for k in counters if k not in ('pending','active_context')),'activation counter rollback')
        require(all(v>=p for v,p in zip(c,previous['native_choices'])),'activation choice rollback')
        if v2:require(all(v>=p for v,p in zip(q,previous['queue_reasons'])),'activation queue reason rollback')
    if final:
        require(not m['requested'] and m['pending']==0 and m['active_context']==0 and m['hooks_disabled'],'activation closure')
        require(m['frames']>0 and m['taa_admitted']>0 and m['table_forced']>0 and m['sharpen_suppressed']>0,'activation was exercised')
    return dict(m,scoped_activation_verified=True,exclusive_aa_verified=False,
                saved_aa_off_observed=c[0]>0)

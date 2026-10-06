"""Check exact-request retirement after publication-blocked native callbacks."""
from arm_continuous_dlaa import require
from continuous_requests import identity

COUNTERS = ('candidates', 'unmatched', 'ambiguous', 'invalid', 'started', 'pending', 'retired', 'failed')

def review(status, previous=None, final=False):
    m=status['forwarded_requests']
    require(m.get('schema') == 1 and m.get('policy') == 'validated-native-return-v1', 'forward retirement policy')
    for k in COUNTERS:
        require(type(m.get(k)) is int and 0 <= m[k] < 2**64-1, 'forward retirement counter '+k)
    require(m['candidates'] == sum(m[k] for k in ('unmatched','ambiguous','invalid','started')), 'forward admission partition')
    require(m['started'] == m['pending']+m['retired']+m['failed'], 'forward completion partition')
    require(m['candidates'] == status['requests']['taa']['blocked'], 'forward callback coverage')
    require(status['fault'] != 0 or m['failed'] == 0, 'forward failure requires runtime fault')
    if m['retired']:
        for k in ('first_retired','last_retired'):
            r=m[k];identity(r)
            require(all(r[p] > 0 for p in ('frame','epoch','sequence','packet','state','view','context','layer')), 'retired request identity')
            require(r['bypasses'] > 0, 'retired request bypass receipt')
        if m['retired'] == 1:
            require(m['first_retired'] == m['last_retired'], 'single retired request')
    else:
        require(m['first_retired'] is None and m['last_retired'] is None, 'empty retirement receipts')
    if previous:
        require(all(m[k] >= previous[k] for k in COUNTERS if k != 'pending'), 'forward retirement counter rollback')
    if final:
        require(m['pending'] == 0, 'pending native consumer at stop')
    return dict(m, native_forwarded_retirements_verified=m['failed'] == 0)

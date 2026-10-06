"""Independent checks for Reset + completed-fence resource retirement counters."""
def require(ok, reason):
    if not ok:
        raise ValueError('recording retention: ' + reason)


def review(s):
    r = s['recordings']
    require(r['schema'] == 1 and r['policy'] == s['retention'] == 'reset-and-fence-v1', 'policy')
    require(r['capacity'] == 128 and s['sdk']['caller_retained'] is True, 'ownership mode')
    keys = ('created', 'submitted', 'retired', 'released', 'live', 'peak',
            'resource_references', 'awaiting_submission', 'awaiting_reset', 'awaiting_gpu')
    require(all(type(r[k]) is int and 0 <= r[k] < 2**64-1 for k in keys), 'counter types')
    prepared=s.get('scaled_route',{}).get('schema')==3
    spatial=s.get('spatial_recordings') if prepared else 0
    route=s.get('route_only_signals') if prepared else 0
    require(all(type(x) is int and 0<=x<2**64-1 for x in (spatial,route)), 'additional recording counters')
    require(r['created'] == s['depth_copies']+spatial, 'injected recording coverage')
    require(r['submitted']+route <= s['signal_value'] <= r['created']+route, 'submission coverage')
    if prepared:require(route<=s['scaled_route']['retained'], 'downstream signal coverage')
    require(r['released'] <= r['retired'] <= r['submitted'] <= r['created'], 'release proof counts')
    require(r['live'] == r['created'] - r['released'] <= r['peak'] <= 128, 'bounded live set')
    require(r['resource_references'] == s['resource_count'] == 4*r['live'], 'resource references')
    require(r['awaiting_submission'] == r['created'] - r['submitted'], 'pending submissions')
    require(r['awaiting_reset'] == r['created'] - r['retired'] <= r['live'], 'replayable generations')
    require(r['live'] - r['awaiting_reset'] <= r['awaiting_gpu'] <= r['submitted'] - r['released'], 'GPU-retirement coverage')
    if s['hooks_disabled']:
        require(r['awaiting_submission'] == r['awaiting_gpu'] == 0, 'stopped GPU drainage')
        require(r['live'] == r['awaiting_reset'], 'only replayable generations pinned at stop')
    return r

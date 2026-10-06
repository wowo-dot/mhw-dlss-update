"""Validate auxiliary replacement admission and bounded receipts."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import integer, vector

COUNTERS = ('entries', 'unchanged', 'refreshed', 'rejected', 'api_checks', 'api_rejected')

def review(t, previous=None):
    require(t['schema'] == 1 and t['policy'] == 'auxiliary-entry-retention-v1', 'auxiliary refresh policy')
    require(all(integer(t[k]) for k in COUNTERS), 'auxiliary refresh counters')
    require(t['entries'] == t['unchanged'] + t['refreshed'] + t['rejected'], 'auxiliary entry accounting')
    require(t['api_rejected'] <= t['api_checks'] <= t['entries'], 'auxiliary API accounting')
    require(integer(t['reason']) and t['reason'] <= 6 and bool(t['reason']) == bool(t['rejected']), 'auxiliary rejection reason')
    vector(t['last'], 7)
    if t['refreshed']:
        frame, stage, reg, old_w, new_w, old_h, new_h = t['last']
        require(frame > 0 and 4 <= stage < 12 and reg in (1, 2) and all(0 < x <= 16384 for x in (old_w, new_w, old_h, new_h)), 'auxiliary refresh dimensions')
    else:
        require(t['last'] == [0] * 7, 'unused auxiliary receipt')
    if previous:
        require(all(t[k] >= previous[k] for k in COUNTERS), 'auxiliary counter rollback')
        if t['refreshed'] == previous['refreshed']:
            require(t['last'] == previous['last'], 'auxiliary receipt changed without admission')
    return deepcopy(t)

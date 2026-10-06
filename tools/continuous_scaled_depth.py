"""Check bounded depth-admission diagnostics; these records never authorize reuse."""
from copy import deepcopy
from continuous_depth import count, vector
from arm_continuous_dlaa import require

LIMITS = (6, 0, 1, 4, 5, 8, 0)


def review(status, previous=None):
    trace = status['scaled_depth_capture']
    require(trace['schema'] == 1 and trace['policy'] == 'bounded-scaled-depth-admission-v1'
            and trace['diagnostic_only'] is True, 'scaled depth diagnostic policy')
    require((trace['capacity'], trace['event_limit'], trace['sample_limit']) == (128, 32, 5), 'scaled depth bounds')
    for key in ('events', 'uncorrelated', 'missing', 'readiness_rejected'):
        count(trace[key], 'scaled depth ' + key)
    counters = trace['counts']
    require(type(counters) is list and len(counters) == 7, 'scaled depth stages')
    for row, limit in zip(counters, LIMITS):
        vector(row, 16, 'scaled depth counters')
        require(not any(row[limit+1:]), 'scaled depth unknown reason')
    require(sum(map(sum, counters)) == trace['events'] + trace['uncorrelated'], 'scaled depth event partition')
    require(trace['missing'] == status['depth_eligibility']['lease']['rejected'], 'scaled depth missing coverage')
    require(trace['missing'] + trace['readiness_rejected'] == status['dispatch_reasons'][7], 'scaled depth readiness partition')
    total, rows = trace['missing'], trace['samples']
    ordinals = list(range(1, min(total, 4) + 1)) + ([total] if total >= 5 else [])
    require(type(rows) is list and [r['ordinal'] for r in rows] == ordinals, 'scaled depth bounded ordinals')
    last_tick = 0
    for row in rows:
        expected = vector(row['expected'], 8, 'scaled depth expected')
        actual = vector(row['actual'], 5, 'scaled depth actual')
        require(all(expected[i] > 0 for i in (0, 1, 2, 3, 5, 6, 7)), 'scaled depth request identity')
        require(expected[0] >= last_tick, 'scaled depth time rollback')
        last_tick = expected[0]
        require(actual[3] <= 1 and actual[4] <= 1, 'scaled depth lease flags')
        require(actual != [expected[6], expected[7], expected[1], 1, 0], 'scaled depth sample is not missing')
        matching = count(row['matching'], 'scaled depth matching', 129)
        overwritten = count(row['ring_overwritten'], 'scaled depth overwritten')
        require(overwritten <= max(0, trace['events'] - 128), 'scaled depth overwrite coverage')
        events = row['events']
        require(type(events) is list and len(events) == min(32, matching), 'scaled depth event bound')
        counts = [[0] * 16 for _ in range(7)]
        for event in events:
            vector(event, 22, 'scaled depth event')
            tick, frame, epoch, _, stage, reason = event[:6]
            require(0 < tick <= expected[0] and (frame, epoch) == tuple(expected[1:3]), 'scaled depth exact frame/epoch')
            require(stage < 7 and reason <= LIMITS[stage], 'scaled depth event kind')
            counts[stage][reason] += 1
        require(all(n <= available for row_counts, totals in zip(counts, counters)
                    for n, available in zip(row_counts, totals)), 'scaled depth event counter coverage')
    if previous is not None:
        require(previous['policy'] == trace['policy'], 'scaled depth policy changed')
        require(all(trace[k] >= previous[k] for k in ('events', 'uncorrelated', 'missing', 'readiness_rejected')), 'scaled depth rollback')
        require(all(a >= b for new, old in zip(counters, previous['counts']) for a, b in zip(new, old)), 'scaled depth counter rollback')
        current = {r['ordinal']: r for r in rows}
        for old in previous['samples']:
            if old['ordinal'] in current:
                require(old == current[old['ordinal']], 'scaled depth sample changed')
    result = deepcopy(trace)
    result['diagnostic_consistency_verified'] = True
    result['menu_root_cause_verified'] = False
    return result

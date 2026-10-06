"""Review qualified native-resolve telemetry; this is not whole-renderer AA coverage."""
from arm_continuous_dlaa import require

COUNTERS = ('candidates', 'pending', 'unclaimed_native', 'claimed', 'reconstructed',
            'unfiltered', 'recovered', 'rejected', 'unrecoverable', 'sdk_commands',
            'shader_checks', 'shader_rejected')
OUTCOMES = ('reconstructed', 'unfiltered', 'recovered', 'rejected', 'unrecoverable')


def review(status, previous=None, final=False):
    r = status['resolve']
    require(r.get('schema') == 1 and r.get('policy') == 'qualified-resolve-no-native-retry-v1'
            and r.get('exclusive_mode') is False, 'resolve policy/scope')
    for key in COUNTERS:
        require(type(r.get(key)) is int and 0 <= r[key] < 2**64-1, 'resolve counter ' + key)
    require(r['candidates'] == r['pending'] + r['unclaimed_native'] + r['claimed'],
            'resolve candidate partition')
    require(r['claimed'] == sum(r[k] for k in OUTCOMES), 'resolve outcome partition')
    require(r['claimed'] <= r['shader_checks'] <= r['candidates'] and
            r['shader_rejected'] <= r['shader_checks'], 'resolve shader coverage')
    require(r['sdk_commands'] <= r['claimed'], 'resolve SDK command coverage')
    # frames increments inside the replacement lock; its outcome is published
    # immediately afterward under another lock. A live snapshot can straddle it.
    require(r['reconstructed'] <= status['frames'] <= r['reconstructed'] + r['pending'],
            'resolve frame agreement')
    require(status['sdk']['evaluations'] == status['frames'], 'resolve SDK evaluations')
    require(status['fault'] == 0 and all(r[k] == 0 for k in
            ('recovered', 'rejected', 'unrecoverable', 'shader_rejected')), 'resolve failure')
    require(r['sdk_commands'] == r['reconstructed'], 'healthy resolve SDK commands')
    if previous is not None:
        require(all(r[k] >= previous[k] for k in COUNTERS if k != 'pending'),
                'resolve counter rollback')
    if final:
        require(r['pending'] == 0 and r['reconstructed'] > 9, 'completed sustained bypass')
    return dict(r, qualified_bypass_telemetry_verified=True,
                whole_renderer_aa_coverage_verified=False)

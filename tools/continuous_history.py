"""Check bounded history telemetry for the legacy and camera-position policies."""
import math
import struct

REASONS = ('first', 'frame_gap', 'epoch_gap', 'camera', 'view', 'depth',
           'viewport', 'projection', 'interval', 'view_other', 'view_translation')


def require(ok, message):
    if not ok:
        raise ValueError('history: ' + message)


def f32(value):
    try:
        return struct.unpack('<f', struct.pack('<f', value))[0]
    except OverflowError:
        return math.copysign(math.inf, value)


def uint(value, bits=64):
    return type(value) is int and 0 <= value < 2**bits


def stamp(s, predecessor=False):
    for key in ('frame', 'epoch', 'microseconds', 'camera', 'view', 'depth'):
        require(uint(s[key]) and (predecessor or s[key] > 0), 'stamp ' + key)
    require(len(s['viewport']) == 2 and all(uint(x, 32) for x in s['viewport']), 'viewport')
    for key in ('view_bits', 'projection_bits'):
        require(len(s[key]) == 16 and all(uint(x, 32) for x in s[key]), key)
    view = [struct.unpack('<f', struct.pack('<I', x))[0] for x in s['view_bits']]
    require(all(math.isfinite(x) for x in view), 'finite view')
    return view


def camera_center(v):
    require(all(math.isfinite(x) for x in v) and [v[i] for i in (3, 7, 11, 15)] == [0, 0, 0, 1], 'affine view')
    r = [v[4*i:4*i+3] for i in range(3)]
    require(all(abs(sum(r[i][k]*r[j][k] for k in range(3)) - int(i == j)) <= 1e-4
                for i in range(3) for j in range(3)), 'orthonormal view')
    determinant = sum(r[0][i] * (r[1][(i+1)%3]*r[2][(i+2)%3] - r[1][(i+2)%3]*r[2][(i+1)%3]) for i in range(3))
    require(abs(determinant - 1) <= 1e-4, 'proper rotation')
    # Independent pivoted solve, rather than the native cofactor implementation.
    a = [[r[j][i] for j in range(3)] + [-v[12+i]] for i in range(3)]
    for k in range(3):
        pivot = max(range(k, 3), key=lambda i: abs(a[i][k]))
        require(abs(a[pivot][k]) > 1e-8, 'nonsingular view')
        a[k], a[pivot] = a[pivot], a[k]
        scale = a[k][k]
        a[k] = [x / scale for x in a[k]]
        for i in range(3):
            if i != k:
                scale = a[i][k]
                a[i] = [x - scale*y for x, y in zip(a[i], a[k])]
    center = [row[3] for row in a]
    require(all(math.isfinite(x) for x in center), 'finite camera')
    require(all(abs(sum(center[i]*r[i][j] for i in range(3)) + v[12+j]) <= 1e-9*(1+abs(v[12+j])) for j in range(3)), 'camera reconstruction')
    return center


def example(e, position_policy=False, avoided=False, comparison=False, suppressed=False):
    p, n = e['previous'], e['next']
    pv, nv = stamp(p, True), stamp(n)
    require(not p['frame'] or n['frame'] > p['frame'], 'forward frame')
    projection = sum(1 << i for i in range(16) if p['projection_bits'][i] != n['projection_bits'][i])
    reasons = 0
    differences = [not p['frame'], n['frame'] != p['frame'] + 1,
                   n['epoch'] != p['epoch'] + 1, n['camera'] != p['camera'],
                   n['view'] != p['view'], n['depth'] != p['depth'],
                   n['viewport'] != p['viewport'], bool(projection)]
    for i, changed in enumerate(differences):
        if changed:
            reasons |= 1 << i
    jumps = 0
    if p['microseconds']:
        require(n['microseconds'] > p['microseconds'], 'forward time')
        if f32(f32(n['microseconds'] - p['microseconds']) / 1000) > 250:
            reasons |= 1 << 8
        for i, (before, after) in enumerate(zip(pv, nv)):
            translation = 12 <= i <= 14
            if abs(f32(after - before)) > (20 if translation else .5):
                jumps |= 1 << i
                if not translation or not position_policy:
                    reasons |= 1 << (10 if translation else 9)
    if position_policy:
        nc = camera_center(nv)
        pc = camera_center(pv) if p['frame'] else [0, 0, 0]
        distance = math.dist(pc, nc) if p['frame'] else 0
        if distance > 20:
            reasons |= 1 << 10
        for key, expected in (('previous_camera', pc), ('next_camera', nc)):
            require(len(e[key]) == 3 and all(math.isclose(x, y, abs_tol=1e-7, rel_tol=1e-9) for x, y in zip(e[key], expected)), 'recomputed ' + key)
        require(math.isclose(e['camera_distance'], distance, abs_tol=1e-7, rel_tol=1e-9), 'recomputed camera distance')
    if not p['frame']:
        reasons = 1
    if comparison:
        removed = reasons == 1 << 10
        require(e['control_reasons'] == reasons and type(e['control_reasons']) is int, 'control reasons')
        require(type(e['displacement_only_suppressed']) is bool and e['displacement_only_suppressed'] == removed, 'suppression decision')
        if removed:
            reasons = 0
    require(uint(e['reasons'], 11) and e['reasons'] == reasons, 'recomputed reasons')
    if suppressed:
        require(comparison and removed and reasons == 0, 'distance-only example')
    else:
        require((position_policy and reasons == 0 and bool(jumps & 0x7000)) if avoided else bool(reasons), 'example reset/avoidance')
    require(e['projection_mask'] == projection and e['view_jump_mask'] == jumps, 'recomputed element masks')


def review(h, evaluations, resets, expected_policy=None):
    comparison = h['policy'] == 'distance-only-comparison-v1'
    position_policy = comparison or h['policy'] == 'camera-displacement-v1'
    require((h['schema'], h['policy']) in ((1, 'v1.18.0-unchanged'), (2, 'camera-displacement-v1'), (3, 'distance-only-comparison-v1')), 'schema/policy')
    require(expected_policy is None or h['policy'] == expected_policy, 'build policy')
    names = REASONS[:-1] + ('camera_displacement',) if position_policy else REASONS
    for key in ('evaluations', 'resets', 'retained', 'retained_streak', 'longest_retained_streak'):
        require(uint(h[key]), 'counter ' + key)
    require(h['evaluations'] == evaluations and h['resets'] == resets, 'SDK counter agreement')
    require(h['retained'] + resets == evaluations, 'evaluation partition')
    require(h['retained_streak'] <= h['longest_retained_streak'] <= h['retained'], 'streak bounds')
    counts = h['counts']
    require(set(counts) == set(names), 'reason keys')
    require(all(uint(v) and v <= resets for v in counts.values()), 'reason counts')
    require(counts['first'] == int(evaluations > 0), 'first evaluation')
    require(resets <= sum(counts.values()) <= resets * len(REASONS), 'overlapping reasons')
    expected = [i for i, name in enumerate(names) if counts[name]]
    require([entry['reason_index'] for entry in h['first']] == expected, 'bounded reason examples')
    for entry in h['first']:
        example(entry['example'], position_policy, comparison=comparison)
        require(entry['example']['reasons'] & (1 << entry['reason_index']), 'example membership')
    if resets:
        example(h['latest'], position_policy, comparison=comparison)
        require(all(entry['example']['next']['frame'] <= h['latest']['next']['frame'] for entry in h['first']), 'example order')
    else:
        require(h['latest'] is None, 'empty examples')
    result = dict(counts=counts, retained=h['retained'], longest_retained_streak=h['longest_retained_streak'])
    if position_policy:
        count = h['translation_resets_avoided']
        require(uint(count) and count <= h['retained'], 'avoided count')
        for key in ('first_avoided', 'latest_avoided'):
            if count:
                example(h[key], True, True, comparison=comparison)
            else:
                require(h[key] is None, 'empty avoided examples')
        if count:
            require(h['first_avoided']['next']['frame'] <= h['latest_avoided']['next']['frame'], 'avoided order')
        result['translation_resets_avoided'] = count
    if comparison:
        require(h['diagnostic_only'] is True, 'diagnostic policy marker')
        count = h['displacement_only_resets_suppressed']
        require(uint(count) and count <= h['retained'], 'suppressed count')
        require(uint(h['control_resets']) and h['control_resets'] == resets + count, 'control reset accounting')
        for key in ('first_displacement_suppressed', 'latest_displacement_suppressed'):
            if count:
                example(h[key], True, comparison=True, suppressed=True)
            else:
                require(h[key] is None, 'empty suppressed examples')
        if count:
            require(h['first_displacement_suppressed']['next']['frame'] <= h['latest_displacement_suppressed']['next']['frame'], 'suppressed order')
        # Other retained examples can independently prove a removed reset.
        if h['translation_resets_avoided']:
            require(count > 0 or not any(h[k]['displacement_only_suppressed'] for k in ('first_avoided','latest_avoided')), 'suppression present in avoided examples')
        result.update(diagnostic_only=True, control_resets=h['control_resets'], displacement_only_resets_suppressed=count)
    return result

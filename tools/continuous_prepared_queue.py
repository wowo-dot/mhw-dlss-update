"""Validate bounded native preparation evidence, never GPU correctness."""
from arm_continuous_dlaa import require
import struct
import math


def uint(value, limit=2**64):
    require(type(value) is int and 0 <= value < limit, 'prepared queue integer')
    return value


def words(value, count, limit=2**64):
    require(type(value) is list and len(value) == count, 'prepared queue array')
    for x in value:
        uint(x, limit)
    return value


def texture(t):
    words(t, 7)
    require(t[6] in (0, 1), 'prepared texture readability')
    if t[6]:
        require(t[0] > 0 and t[1] > 0 and 0 < t[2] < 65536 and 0 < t[3] < 16384,
                'prepared texture metadata')
        uint(t[4], 2**32); uint(t[5], 2**32)
    else:
        require(t[1:] == [0]*6, 'unknown texture contains invented metadata')


def commands(c, packets):
    opcodes = words(c['opcodes'], 32, 65536)
    require(sum(opcodes) <= packets, 'prepared opcode coverage')
    draws = c['draws']
    require(type(draws) is list and len(draws) == min(opcodes[0], 2), 'prepared draw prefix')
    for d in draws:
        words(d, 6)
        require(d[0] > 0 and d[1] > 0 and all(x < 2**32 for x in d[2:]), 'prepared draw metadata')
    if 'details' in c:
        require(len(c['details'])==len(draws),'prepared draw detail coverage')
        for d in c['details']:
            require(type(d['readable']) is bool,'prepared attachment readability')
            words(d['bindings'],4);words(d['arguments'],2,2**32)
            words(d['target'],6);words(d['pixel'],7)
            require(d['pixel'][6] in (0,1),'prepared shader readability')
            if d['readable']:require(all(d['target'][i] for i in (1,2,3,4)),'prepared attachment target')
            if d['pixel'][6]:require(all(d['pixel'][:5]) and 52<=d['pixel'][5]<=262144,'prepared shader descriptor')


def capture(c):
    stamp = words(c['stamp'], 9)
    require(all(stamp[i] > 0 for i in (0, 2, 3, 4, 5, 8)) and stamp[6] <= 1 and stamp[7] <= 1,
            'prepared identity')
    packets = uint(c['packets'], 65536)
    rect = words(c['rectangle'], 4, 16385)
    require(rect[0] < rect[2] and rect[1] < rect[3], 'prepared active rectangle')
    for bits in words(c['scale_bits'], 3, 2**32):
        scale, = struct.unpack('<f', struct.pack('<I', bits))
        require(math.isfinite(scale) and 0 < scale <= 4, 'prepared scale domain')
    require(type(c['scratch']) is list and len(c['scratch']) == 2, 'prepared scratch slots')
    for t in c['scratch']:
        texture(t)
    entries = c['entries']
    require(type(entries) is list and 0 < len(entries) <= 16, 'prepared descriptor budget')
    seen = set(); temporal = 0; current = None; breaks = 0
    for e in entries:
        identity = words(e['identity'], 5)
        descriptor, phase, order, unit, _ = identity
        require(descriptor > 0 and descriptor not in seen and phase <= 31 and order <= 0x7ffffff,
                'prepared descriptor identity')
        seen.add(descriptor)
        words(e['flags'], 3, 256)
        texture(e['input']); texture(e['output']); commands(e['commands'], packets)
        if descriptor == stamp[5] + 0x140:
            require(unit == stamp[5] and phase == 15 and order == 2, 'prepared temporal boundary')
            temporal += 1
            current = e['output'][0]
        elif current is not None:
            if not current or e['input'][0] != current or not e['output'][0]:
                breaks += 1
            current = e['output'][0]
    require(temporal == 1 and uint(c['chain_breaks']) == breaks, 'prepared logical adjacency accounting')
    commands(c['final_commands'], packets)


def review(status, previous=None):
    r = status['scaled_route']['prepared_queue']
    require(r['schema'] == 1 and r['policy'] == 'native-prepared-color-chain-v1' and
            r['diagnostic_only'] is True and r['interval'] == 128 and r['capacity'] == 16,
            'prepared queue schema')
    visits, attempts, accepted = (uint(r[k]) for k in ('visits', 'attempts', 'accepted'))
    results = words(r['results'], 6)
    require(attempts == (visits+127)//128 and sum(results) == attempts and results[0] == accepted,
            'prepared queue accounting')
    samples = r['samples']
    require(type(samples) is list and len(samples) == min(accepted, 16)+(accepted > 16),
            'prepared queue sample coverage')
    last_attempt = 0
    for s in samples:
        attempt, visit = uint(s['attempt']), uint(s['visit'])
        require(last_attempt < attempt <= attempts and visit == (attempt-1)*128+1 and visit <= visits,
                'prepared sample ordering')
        last_attempt = attempt
        capture(s['capture'])
    if previous:
        require(all(r[k] >= previous[k] for k in ('visits', 'attempts', 'accepted')) and
                all(a >= b for a, b in zip(results, previous['results'])), 'prepared counter rollback')
        n = min(previous['accepted'], 16)
        require(samples[:n] == previous['samples'][:n], 'prepared immutable prefix')
        if accepted == previous['accepted']:
            require(samples == previous['samples'], 'prepared receipt changed without capture')
    return r

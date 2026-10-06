"""Validate bounded CPU enqueue recipes; never attest GPU bindings or states."""
import math
import struct
from copy import deepcopy
from arm_continuous_dlaa import require

SHADERS = [(712, 'c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6'),
           (14596, '4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69')]


def integers(values, n, limit=2**64):
    require(isinstance(values, list) and len(values) == n, 'haze field length')
    require(all(type(v) is int and 0 <= v < limit for v in values), 'haze integer range')


def review(status, previous=None, final=False):
    r = status['scaled_route']['haze_capture']
    require((r['schema'], r['policy']) in ((1, 'bounded-haze-enqueue-v1'), (2, 'bounded-haze-enqueue-v2'))
            and r['diagnostic_only'] is True, 'haze capture policy')
    require(r['capacity'] == 3 and r['attempt_limit'] == 16, 'haze capture bounds')
    integers([r['attempts'], r['count'], r['phase'], r['reason']], 4)
    require(0 <= r['count'] <= 3 and r['count'] <= r['attempts'] <= 16, 'haze counts')
    require(type(r['entered']) is bool and r['phase'] <= 7 and r['reason'] <= 11, 'haze phase')
    integers(r['rejected'], 12)
    require(r['rejected'][0] == 0, 'haze zero rejection')
    # Reentrant callbacks can reject an in-progress attempt separately. Do not
    # equate attempt counts to rejection counts without that qualification.
    require(r['count'] + sum(r['rejected'][2:]) <= r['attempts'], 'haze accounting')
    require(len(r['samples']) == r['count'], 'haze retained sample count')
    prior_frame = 0
    for q in r['samples']:
        integers(q['at'], 8)
        require(q['at'][1] > prior_frame and all(q['at'][j] for j in (0, 1, 2, 3, 4, 7)), 'haze frame identity')
        prior_frame = q['at'][1]
        integers(q['queue'], 4)
        require(q['queue'][0] and q['queue'][3] and q['queue'][2] == q['queue'][1] + 1
                and q['queue'][1] < 16384, 'haze exact queue append')
        for name in ('source', 'destination'):
            integers(q[name], 13)
            require(q[name][0] and q[name][5] and q[name][6] and q[name][12], 'haze texture metadata')
        require(q['source'][0] != q['destination'][0], 'haze distinct owners')
        integers(q['named_ids'], 5, 2**32)
        integers(q['named_objects'], 5)
        require(q['named_objects'][0] == q['source'][0], 'haze named color source')
        integers(q['flags'], 4, 256)
        integers([q['strength_bits']], 1, 2**32)
        integers(q['rectangle'], 4, 2**32)
        integers(q['dimensions'], 2, 2**32)
        integers(q['offset_bits'], 2, 2**32)
        require(len(q['draw_rectangle']) == 4 and all(type(x) is int for x in q['draw_rectangle']), 'haze draw rectangle')
        integers(q['arguments'], 2, 2**32)
        require(q['arguments'][0] in (3, 4) and q['arguments'][1] <= 1048576, 'haze draw arguments')
        integers(q['vertices_bits'], 4*q['arguments'][0], 2**32)
        require(all(math.isfinite(struct.unpack('<f', struct.pack('<I', x))[0])
                    for x in q['vertices_bits']), 'haze finite vertices')
        require([(v['size'], v['sha256']) for v in q['shaders']] == SHADERS, 'haze shader identities')
        require(len(q['layout']) == 2, 'haze input count')
        for item in q['layout']:
            require(0 < len(item['semantic']) < 64 and all(32 <= ord(c) <= 126 for c in item['semantic']), 'haze semantic')
            integers(item['fields'], 6, 2**32)
        for name, size in (('depth', 13), ('blend', 82), ('raster', 11)):
            integers(q[name], size, 2**32)
        owners = q['owners']
        require(0 < len(owners) <= 64, 'haze owner table bound')
        for item in owners:
            integers(item, 3)
            require(item[0] < 2**32 and item[1] < 256, 'haze owner fields')
        if r['schema'] == 1:
            # Historical v1 captures used an incorrect three-range layout.
            require(len(q['ranges']) == 2, 'haze stage ranges')
            for parts in q['ranges']:
                require(len(parts) == 3, 'haze range pairs')
                for part in parts:
                    require(type(part['start']) is int and 0 <= part['start'] <= 4096
                            and len(part['words']) <= 128 and part['start']+len(part['words']) <= 4096, 'haze packed range')
                    integers(part['words'], len(part['words']), 2**32)
            pixel_words = q['ranges'][1][0]['words']
        else:
            require(len(q['stage_bindings']) == 2, 'haze stage bindings')
            for part in q['stage_bindings']:
                integers([part['count'], part['start']], 2, 65536)
                require(part['count'] == len(part['words']) <= 128
                        and (not part['count'] or part['start']+part['count'] <= 4096), 'haze packed range')
                integers(part['metadata'], 8, 256)
                integers(part['words'], part['count'], 2**32)
            pixel_words = q['stage_bindings'][1]['words']
        words = {}
        for word in pixel_words:
            kind, index, register = word & 15, (word >> 4) & 255, word >> 24
            require(kind <= 3 and (kind == 0 or index < len(owners)), 'haze packed owner index')
            if kind == 2:
                require(register < 4 and register not in words, 'haze texture register')
                words[register] = index
        require(set(words) == set(range(4)) and len(q['textures']) == 4, 'haze four SRVs')
        for reg, item in enumerate(q['textures']):
            require(item['register'] == reg and item['index'] == words[reg]
                    and item['view'] > 0 and item['cpu_descriptor'] > 0, 'haze SRV identity')
            integers(item['owner'], 13)
            selected = owners[item['index']]
            require(item['owner'][0] == selected[2] and item['owner'][12], 'haze resource owner')
            require(any(q['named_ids'][i] != 2**32-1 and q['named_ids'][i] & 4095 == selected[0] & 4095
                        and q['named_objects'][i] == selected[2] for i in ([3, 4] if reg == 3 else [reg])), 'haze named binding')
        require(q['textures'][0]['owner'] == q['source'], 'haze source metadata agreement')
        require(q['same_frame_enqueued'] is True and q['gpu_bindings_verified'] is False
                and q['resource_states_verified'] is False, 'haze limited claims')
    if previous is not None:
        require(r['attempts'] >= previous['attempts'] and r['count'] >= previous['count'], 'haze counters rollback')
        require(all(a >= b for a, b in zip(r['rejected'], previous['rejected'])), 'haze rejection rollback')
        require(r['samples'][:previous['count']] == previous['samples'], 'haze retained recipe changed')
    if final:
        require(r['entered'] is False, 'haze callback left active')
    return deepcopy(r)

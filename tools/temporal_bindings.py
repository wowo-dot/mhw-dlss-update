"""Check bounded native binding snapshots; this does not prove restoration."""
from temporal_shader import require


def uint(value, bits=64, positive=False):
    return type(value) is int and int(positive) <= value < 2**bits


def review_root(root, heaps):
    require(uint(root['signature']), 'binding root identity')
    args = root['arguments']
    require(isinstance(args, list) and len(args) <= 64, 'binding argument budget')
    require(root['signature'] or not args, 'arguments without root signature')
    indices = []
    for arg in args:
        index, kind = arg['index'], arg['kind']
        require(uint(index, 6) and uint(kind, 3, True) and kind <= 5, 'binding argument type')
        require(uint(arg['address']) and uint(arg['mask']), 'binding argument value')
        indices.append(index)
        words = arg['constants']
        require(isinstance(words, list) and len(words) <= 64, 'binding constant budget')
        if kind == 2:
            require(arg['address'] == 0 and words, 'binding constant storage')
            require(all(isinstance(p, list) and len(p) == 2 and uint(p[0], 6) and uint(p[1], 32) for p in words), 'binding constant words')
            offsets = [p[0] for p in words]
            require(offsets == sorted(set(offsets)) and arg['mask'] == sum(1 << p for p in offsets), 'binding constant mask')
        else:
            require(arg['mask'] == 0 and not words, 'descriptor constant storage')
            if kind == 1:
                require(heaps and arg['address'] > 0, 'descriptor table without heap/address')
    require(indices == sorted(set(indices)), 'binding argument order/duplicates')


def review_bindings(capture, *, sample_count=9, hook_count=18):
    require((sample_count,hook_count) in ((9,18),(3,23)), 'binding sample budget')
    trace = capture['native_bindings']
    require(trace['schema'] == 1 and trace['fault'] == trace['pending'] == 0, 'binding observer status')
    require(trace['hooks_disabled'] is True and trace['drained'] is True, 'binding observer closure')
    require(trace['hook_count'] == hook_count and uint(trace['list_count'], 8, True) and trace['list_count'] <= 128, 'binding observer bounds')
    records = trace['records']
    native = capture['native_compute']['records']
    submitted = capture['submission']['samples']
    require(len(records) == len(native) == len(submitted) == sample_count, 'binding snapshot count')
    lists = set()
    for index, (r, n, s) in enumerate(zip(records, native, submitted)):
        require(r['ready'] is True and r['receipt'] == n['receipt'] == s['receipt'] == index, 'binding receipt readiness')
        require(uint(r['frame'], positive=True) and r['frame'] == n['frame'] == s['frame'], 'binding frame association')
        require(uint(r['command'], positive=True) and r['command'] == n['command'] == s['command'], 'binding command association')
        require(uint(r['generation'], positive=True) and r['generation'] == s['reset'], 'binding reset generation')
        require(uint(r['pipeline'], positive=True) and r['pipeline'] == r['expected_pipeline'] == n['binding_pipeline'], 'binding pipeline cache match')
        require(uint(r['expected_root'], positive=True) and r['compute']['signature'] == r['expected_root'] == n['binding_root'], 'binding root cache match')
        heaps = r['heaps']
        require(isinstance(heaps, list) and 1 <= len(heaps) <= 2 and all(uint(p, positive=True) for p in heaps) and len(set(heaps)) == len(heaps), 'binding descriptor heaps')
        review_root(r['compute'], heaps)
        review_root(r['graphics'], heaps)
        require(any(a['index'] == 0 and a['kind'] == 1 for a in r['compute']['arguments']), 'native TAA descriptor table')
        lists.add(r['command'])
    require(len(lists) <= trace['list_count'], 'binding list registry count')
    return dict(native_binding_snapshots_verified=True, native_binding_snapshots=sample_count,
                native_binding_lists=len(lists), native_state_restoration_verified=False,
                presentation_changed=False)

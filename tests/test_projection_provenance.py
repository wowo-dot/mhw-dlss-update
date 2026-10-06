"""Exercise journal replay with real owned SDK captures and independent damage."""
import copy
import hashlib
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import projection_provenance as pv
import review_projection_binding as review
import arm_projection_binding as control


class Provenance(unittest.TestCase):
    def data(self, case='roundtrip'):
        path = Path(sys.argv_fixture) / (case + '.json')
        t = json.loads(path.read_text())
        p = json.loads(Path(str(path) + '.ready.jsonl').read_text())
        r = dict(dimension=3, format=39, flags=2, samples=1, mips=1, depth_or_array=1,
                 resource=t['marker']['resource'], identity=t['marker']['identity'])
        return p, t, r, copy.deepcopy(t['marker'])

    def check_result(self, case='roundtrip', mutate=None):
        data = self.data(case)
        if mutate:
            mutate(*data)
        return pv.validate(*data)

    def partial(self, change, state_only=False):
        result = self.check_result(mutate=change)
        self.assertEqual(result['view_descriptor_provenance_verified'], state_only)
        self.assertFalse(result['local_depth_state_declared'])
        self.assertTrue(result['provenance_reasons'])

    def reject(self, change):
        with self.assertRaises((ValueError, KeyError)):
            self.check_result(mutate=change)

    def test_roundtrip(self):
        r = self.check_result()
        self.assertTrue(r['view_descriptor_provenance_verified'])
        self.assertTrue(r['local_depth_state_declared'])
        self.assertEqual(r['depth_view_descriptor'], [40, 3, 0, 0, 0, 0])
        self.assertEqual(r['local_depth_transitions'], 1)
        self.assertEqual(r['local_depth_clears'], 1)
        for k in ('resource_state_at_gpu_execution_verified', 'submission_verified', 'gpu_contents_verified', 'raster_jitter_verified'):
            self.assertFalse(r[k])

    def test_clear_float_register(self):
        _, t, _, _ = self.data()
        clear = next(e for e in t['history'] if e['kind'] == 6)
        self.assertEqual(clear['descriptor'][:3], [1, struct.unpack('<I', struct.pack('<f', .375))[0], 0])

    def test_adjusted_identity(self):
        _, t, _, _ = self.data()
        self.assertNotEqual(t['marker']['resource'], t['marker']['identity'])
        self.assertTrue(self.check_result()['view_descriptor_provenance_verified'])

    def test_reused_handle_new_lifetime(self):
        self.assertTrue(self.check_result('reuse')['local_depth_state_declared'])

    def test_missing_or_invalidated_views(self):
        for case in ('prewatch', 'destroyed', 'overwrite', 'copy_simple', 'copy_ranges', 'copy_implicit', 'readonly'):
            with self.subTest(case=case):
                r = self.check_result(case)
                self.assertFalse(r['view_descriptor_provenance_verified'])
                self.assertFalse(r['local_depth_state_declared'])

    def test_missing_generation_evidence(self):
        for case in ('no_reset', 'no_barrier', 'split', 'reset_after_barrier', 'closed'):
            with self.subTest(case=case):
                r = self.check_result(case)
                self.assertTrue(r['view_descriptor_provenance_verified'])
                self.assertFalse(r['local_depth_state_declared'])

    def test_runtime_failures(self):
        for case in ('heap_changed', 'device_changed', 'expired', 'changed', 'exception', 'capacity', 'pin_capacity', 'deadline_slot'):
            with self.subTest(case=case), self.assertRaises(ValueError):
                self.check_result(case)

    def test_expired_nested_reservation_does_not_publish_parent(self):
        _, t, _, _ = self.data('deadline_slot')
        self.assertEqual(len(t['watch']), 1)
        e = t['watch'][0]
        self.assertEqual(e['kind'], 0)
        self.assertTrue(e['returned'] and e['valid'])
        self.assertGreater(e['handle'], 0)
        self.assertGreater(e['resource'], 0)

    def test_no_marker_not_promoted(self):
        for case in ('pending', 'concurrent'):
            with self.subTest(case=case), self.assertRaises(ValueError):
                self.check_result(case)

    def test_cleanup(self):
        for key, value in (('pending', 1), ('pins', 1), ('error', 2), ('released', False), ('disabled', False)):
            with self.subTest(key=key):
                self.reject(lambda p, t, r, m: t.update({key: value}))

    def test_caps(self):
        for key in ('watch_reserved', 'history_reserved', 'barriers_reserved'):
            with self.subTest(key=key):
                self.reject(lambda p, t, r, m: t.update({key: t[key] + 1}))

    def test_changed_marker(self):
        for key in ('command', 'view', 'handle', 'resource', 'identity'):
            with self.subTest(key=key):
                self.reject(lambda p, t, r, m: t['marker'].update({key: m[key] + 1}))

    def test_duplicate_order(self):
        self.reject(lambda p, t, r, m: t['watch'][1].update(enter=t['watch'][0]['enter']))

    def test_bad_interval(self):
        self.reject(lambda p, t, r, m: t['watch'][1].update(exit=t['watch'][1]['enter'] - 1))

    def test_exception_event(self):
        self.reject(lambda p, t, r, m: t['watch'][1].update(returned=False))

    def test_deadline(self):
        self.reject(lambda p, t, r, m: t['history'][0].update(tick=t['frame_started'] + 2000))

    def test_device_mismatch(self):
        self.reject(lambda p, t, r, m: t['watch'][1].update(object=p['device'] + 8))

    def test_null_default(self):
        self.partial(lambda p, t, r, m: t['watch'][1].update(null_desc=True))

    def test_constructor_wrong_parent(self):
        self.partial(lambda p, t, r, m: t['watch'][1].update(parent=2))

    def test_constructor_wrong_thread(self):
        self.partial(lambda p, t, r, m: t['watch'][1].update(thread=123))

    def test_identity_mismatch(self):
        self.partial(lambda p, t, r, m: t['watch'][1].update(identity=m['identity'] + 8))

    def test_readonly(self):
        self.partial(lambda p, t, r, m: t['watch'][1]['descriptor'].__setitem__(2, 1))

    def test_unsupported_format(self):
        self.partial(lambda p, t, r, m: t['watch'][1]['descriptor'].__setitem__(0, 45))

    def test_mip_bounds(self):
        self.partial(lambda p, t, r, m: t['watch'][1]['descriptor'].__setitem__(3, 1))

    def test_heap_replacement_range(self):
        self.partial(lambda p, t, r, m: p.update(heap_start=p['heap_start'] + p['stride']))

    def test_handle_alignment(self):
        self.partial(lambda p, t, r, m: p.update(heap_start=p['heap_start'] - 1))

    def test_cached_bind(self):
        self.partial(lambda p, t, r, m: t['marker'].update(bind_enter=0, bind_exit=0))

    def test_alias(self):
        self.partial(lambda p, t, r, m: t['barriers'].__setitem__(0, [1, 0, m['resource'], 0, 0, 0, 0]), True)

    def test_bundle(self):
        self.partial(lambda p, t, r, m: t['history'][2].update(kind=7), True)

    def test_other_list_not_merged(self):
        self.partial(lambda p, t, r, m: t['history'][1].update(object=m['command'] + 8), True)

    def test_other_subresource(self):
        self.partial(lambda p, t, r, m: t['barriers'][0].__setitem__(4, 1), True)

    def test_reset_failure(self):
        self.partial(lambda p, t, r, m: t['history'][0].update(result=0x80004005), True)

    def test_end_without_begin(self):
        self.partial(lambda p, t, r, m: t['barriers'][0].__setitem__(1, 2), True)

    def pair(self, t, before, flags=(0, 0)):
        b = t['barriers'][0]
        a = list(b); a[1] = flags[0]
        b = list(b); b[1] = flags[1]; b[5] = before
        t['barriers'] = [a, b]
        t['barriers_reserved'] = 2
        t['history'][1]['count'] = 2

    def test_transition_continuity(self):
        self.partial(lambda p, t, r, m: self.pair(t, 0x80), True)

    def test_transition_continuity_success(self):
        result = self.check_result(mutate=lambda p, t, r, m: self.pair(t, 0x10))
        self.assertTrue(result['local_depth_state_declared'])

    def test_split_pair(self):
        result = self.check_result(mutate=lambda p, t, r, m: self.pair(t, 0x80, (1, 2)))
        self.assertTrue(result['local_depth_state_declared'])

    def test_mismatched_split_pair(self):
        self.partial(lambda p, t, r, m: self.pair(t, 0x40, (1, 2)), True)

    def test_copy_overflow(self):
        p, t, r, m = self.data('copy_simple')
        next(e for e in t['watch'] if e['kind'] == 4)['ranges'] = [[2**64 - 1, 1]]
        with self.assertRaises(ValueError): pv.validate(p, t, r, m)

    def test_list_overlap(self):
        self.partial(lambda p, t, r, m: t['history'][0].update(exit=100), True)

    def test_array_subresource(self):
        r = dict(dimension=3, format=39, flags=2, samples=1, mips=3, depth_or_array=4)
        self.assertEqual(pv.descriptor([40, 4, 0, 2, 3, 1], r), 11)
        for desc in ([40, 4, 0, 3, 3, 1], [40, 4, 0, 2, 4, 1], [40, 4, 0, 2, 3, 2]):
            with self.assertRaises(ValueError): pv.descriptor(desc, r)

    def test_native_caller(self):
        p, t, r, m = self.data()
        result = pv.validate(p, t, r, m, base=1)
        self.assertFalse(result['view_descriptor_provenance_verified'])

    def test_companion_loader(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'binding.jsonl'
            companion = path.with_name('binding-provenance.json')
            data = b'{"schema":1}\n'; companion.write_bytes(data)
            record = dict(file=companion.name, sha256=hashlib.sha256(data).hexdigest())
            self.assertEqual(pv.load(path, record), dict(schema=1))
            for change in (dict(file='../binding-provenance.json'), dict(sha256='a'*64)):
                with self.assertRaises(ValueError): pv.load(path, dict(record, **change))

    def test_no_companion_cannot_review(self):
        path = Path(sys.argv_fixture).parent / 'core-final' / 'attach_depth.jsonl'
        records = review.parse(path.read_bytes())
        records[0].update(version='0.4.0', experiment='projection-view-state-readonly')
        review.one(records, 'ready').update(hooks_installed=True, core_hooks_installed=False)
        with self.assertRaisesRegex(ValueError, 'missing provenance companion'):
            review.review(records, lambda n: path.with_name(path.stem+f'-shader-{n}.bin').read_bytes())

    def test_controller_ready(self):
        path = Path(sys.argv_fixture).parent / 'core-final' / 'attach_depth.jsonl'
        rows = review.parse(path.read_bytes())
        rows[0].update(version='0.4.0', experiment='projection-view-state-readonly', owned_fixture=False, base=1)
        ready_index = rows.index(review.one(rows, 'ready'))
        rows = rows[:ready_index+1]
        rows[-1].update(hooks_installed=True, core_hooks_installed=False)
        p, _, _, _ = self.data(); rows.insert(-1, p)
        # Owned pool addresses are unrelated to a real host: exercise readiness
        # separately from the established reservation verifier.
        import unittest.mock
        with unittest.mock.patch.object(control, 'reservation'):
            control.validate(rows, 1, rows[0]['pid'], rows[0]['creation'])
            rows[-1]['core_hooks_installed'] = True
            with self.assertRaises(ValueError): control.validate(rows, 1, rows[0]['pid'], rows[0]['creation'])

    def combined(self):
        # Synthetic composition of two owned traces tests the review boundary.
        # It is not represented as a native end-to-end capture or game evidence.
        path = Path(sys.argv_fixture).parent / 'core-final' / 'attach_depth.jsonl'
        records = review.parse(path.read_bytes())
        records[0].update(version='0.4.0', experiment='projection-view-state-readonly', base=0x140000000)
        d = review.one(records, 'draw'); v = d['attachments']['ready']['views'][8]
        r = next(r for r in d['attachments']['resources'] if r['resource'] == v['resource'])
        p, t, _, m = self.data()
        remap = {m['command']: d['command'], m['view']: v['backend'], m['resource']: r['resource'],
                 m['identity']: r['identity'], m['handle']: v['handle'], p['device']: d['device']}
        for row in [t['marker'], *t['watch'], *t['history']]:
            for key in ('object', 'command', 'view', 'resource', 'identity', 'handle'):
                if key in row: row[key] = remap.get(row[key], row[key])
        for b in t['barriers']:
            b[2] = remap.get(b[2], b[2]); b[3] = remap.get(b[3], b[3])
        t['watch'][1]['caller'] = records[0]['base'] + 0x24e6516
        p.update(device=d['device'], backend=review.one(records, 'armed')['backend'], heap_start=v['handle'])
        review.one(records, 'armed')['device'] = d['device']
        ready = review.one(records, 'ready'); ready.update(hooks_installed=True, core_hooks_installed=False)
        records.insert(records.index(ready), p)
        return records, lambda n: path.with_name(path.stem+f'-shader-{n}.bin').read_bytes(), t

    def test_combined_review(self):
        result = review.review(*self.combined())
        self.assertTrue(result['view_descriptor_provenance_verified'])
        self.assertTrue(result['local_depth_state_declared'])
        self.assertTrue(result['resource_identity_roundtrip_verified'])
        self.assertFalse(result['submission_verified'])

    def test_combined_marker_disagrees(self):
        rows, blobs, trace = self.combined(); trace['marker']['resource'] += 8
        with self.assertRaisesRegex(ValueError, 'marker mismatch'): review.review(rows, blobs, trace)

    def test_combined_cached_bind_marker(self):
        rows, blobs, trace = self.combined(); trace['marker'].update(bind_enter=0, bind_exit=0)
        with self.assertRaisesRegex(ValueError, 'marker count'): review.review(rows, blobs, trace)


if __name__ == '__main__':
    sys.argv_fixture = sys.argv.pop(1)
    unittest.main()

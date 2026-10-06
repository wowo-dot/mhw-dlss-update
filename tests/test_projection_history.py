"""Review real owned history pressure/overflow and version-bound the new limit."""
import copy
import json
from pathlib import Path
import sys
import unittest

root=Path(sys.argv.pop(1)) if len(sys.argv)>1 else Path('local/projection-binding-20260923/v0.9.1/fixtures-final')
import test_projection_triplet as fixtures
import projection_provenance as pv
import review_projection_binding as review
import arm_projection_binding as control
fixtures.root=root

class History(unittest.TestCase):
    def setUp(self):
        self.args=list(fixtures.Triplet().full('readback_sequence_history_pressure',5))
        self.p=review.one(self.args[0],'provenance_ready')
    def reject(self,edit):
        edit()
        with self.assertRaises((ValueError,KeyError,StopIteration)):review.review(*self.args)
    def arm_records(self):
        rows=copy.deepcopy(self.args[0])
        rows=rows[:rows.index(review.one(rows,'armed'))]
        rows[0].pop('owned_fixture',None)
        # The armer rejects owned-process addresses by design. Use a synthetic
        # host-relative reservation only for its pure contract validator.
        base=rows[0]['base']
        review.one(rows,'trampoline_reservation').update(origin=base+0x228f3b0,page=base+0x10000,slot=base+0x10040)
        return rows
    def test_five_readbacks_beyond_old_limit(self):
        result=review.review(*self.args)
        self.assertEqual(self.args[0][0]['version'],'0.9.1')
        self.assertTrue(result['depth_sequence_readbacks_completed'])
        self.assertGreater(self.args[2]['history_reserved'],6000)
        self.assertLess(self.args[2]['history_reserved'],8192)
        self.assertFalse(result['raster_jitter_verified'])
    def test_native_exact_bound_and_closed_admission(self):
        t=json.loads((root/'history_capacity.json').read_text())
        self.assertEqual(t['error'],1)
        self.assertEqual(t['history_reserved'],8193)
        self.assertEqual(len(t['history']),8192)
        self.assertTrue(all(e['valid'] and e['returned'] and e['kind']==13 for e in t['history']))
        orders=[x for e in t['history'] for x in (e['enter'],e['exit'])]
        self.assertEqual(len(set(orders)),16384)
        self.assertEqual(t['pending'],0);self.assertTrue(t['released'] and t['disabled'])
    def test_native_overflow_quarantines_all_five(self):
        path=root/'readback_sequence_history_overflow.json'
        t=json.loads(path.read_text());p=json.loads(Path(str(path)+'.ready.jsonl').read_text())
        self.assertEqual(t['error'],1);self.assertEqual(t['history_reserved'],8193)
        self.assertEqual(len(t['history']),8192)
        with self.assertRaisesRegex(ValueError,'unclean journal'):pv.structure(p,t)
        for stage in range(5):
            d=json.loads(Path(str(path)+('.readback.json' if stage==1 else f'.depth.f32.control-{stage}.json')).read_text())
            self.assertEqual(d['phase'],9);self.assertFalse(d['mapped'] or d['recording_retired'])
            self.assertTrue(d['resources_retained'])
            self.assertEqual(d['queue_signal_attempts'],1);self.assertEqual(d['completed_value'],1)
            self.assertFalse(Path(str(path)+('.depth.f32' if stage==1 else f'.depth.f32.control-{stage}.f32')).exists())
    def test_new_version_old_capacity_rejected(self):self.reject(lambda:self.p.update(history_capacity=4096))
    def test_old_version_new_capacity_rejected(self):self.reject(lambda:self.args[0][0].update(version='0.9.0'))
    def test_unbounded_capacity_rejected(self):self.reject(lambda:self.p.update(history_capacity=16384))
    def test_capacity_type_rejected(self):self.reject(lambda:self.p.update(history_capacity=True))
    def test_static_storage_bound(self):self.reject(lambda:self.p.update(storage_bytes=12*1024*1024+1))
    def test_dropped_history_entry_rejected(self):self.reject(lambda:self.args[2]['history'].pop(1))
    def test_overflow_count_rejected(self):self.reject(lambda:self.args[2].update(history_reserved=8193))
    def test_new_armer_accepts_matching_contract(self):
        rows=self.arm_records();h=rows[0]
        control.validate(rows,1,h['pid'],h['creation'])
    def test_armer_rejects_version_capacity_mismatch(self):
        for version,capacity in [('0.9.0',8192),('0.9.1',4096)]:
            rows=self.arm_records();rows[0]['version']=version
            review.one(rows,'provenance_ready')['history_capacity']=capacity
            with self.subTest(version=version),self.assertRaisesRegex(ValueError,'history capacity version'):
                control.validate(rows,1,rows[0]['pid'],rows[0]['creation'])

if __name__=='__main__':unittest.main()

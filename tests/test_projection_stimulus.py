"""Review real owned-fixture stimulus traces, including independent rejection cases."""
import copy
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import review_projection_binding as review
import arm_projection_binding as control


class Stimulus(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory=Path(sys.argv_fixture)
        cls.rows=review.parse((cls.directory/'jitter_chain.jsonl').read_bytes())

    def load(self,n):
        return (self.directory/f'jitter_chain-shader-{n}.bin').read_bytes()

    def changed(self,fn):
        rows=copy.deepcopy(self.rows)
        fn(rows)
        with self.assertRaises(ValueError):review.review(rows,self.load)

    def test_chain(self):
        r=review.review(self.rows,self.load)
        self.assertTrue(r['stimulus_cpu_sequence_verified'])
        self.assertTrue(r['nonzero_matrix_binding_observed'])
        self.assertFalse(r['nonzero_known_vertex_binding_correlated']) # Owned synthetic DXBC, not the game VS.
        self.assertEqual(r['cpu_frames'],[10,11,12])
        self.assertGreater(r['view_projection_xy_delta_max'][1],0)
        self.assertFalse(r['raster_jitter_verified'])

    def test_recovery_before_draw(self):
        rows=review.parse((self.directory/'jitter_recovery_first.jsonl').read_bytes())
        result=review.review(rows,lambda n:(self.directory/f'jitter_recovery_first-shader-{n}.bin').read_bytes())
        self.assertTrue(result['stimulus_cpu_sequence_verified'])
    def test_deferred_recovery(self):
        rows=review.parse((self.directory/'jitter_deferred.jsonl').read_bytes())
        result=review.review(rows,lambda n:(self.directory/f'jitter_deferred-shader-{n}.bin').read_bytes())
        self.assertTrue(result['stimulus_cpu_sequence_verified'])
        self.assertEqual(result['cpu_frames'],[10,11,13])
        self.assertEqual(result['recovery_admissions_deferred'],1)
    def test_deferral_bound(self):
        self.changed(lambda r:review.one(r,'stimulus').update(recovery_deferrals=65))
    def test_deferral_missing(self):
        self.changed(lambda r:review.one(r,'stimulus').pop('recovery_deferrals'))
    def test_legacy_version(self):
        rows=copy.deepcopy(self.rows);rows[0]['version']='0.2.0';rows[0].pop('max_recovery_deferrals')
        review.one(rows,'stimulus').pop('recovery_deferrals')
        rows=review.parse(('\n'.join(map(json.dumps,rows))+'\n').encode())
        self.assertTrue(review.review(rows,self.load)['stimulus_cpu_sequence_verified'])

    def test_restoration(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][1].update(restored=False))
    def test_conflict(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][1].update(restored_bits=1234))
    def test_budget(self):
        self.changed(lambda r:review.one(r,'stimulus').update(injections=2))
    def test_missing_recovery(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'].pop())
    def test_repeated_frame(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][2]['source'].update(frame=11))
    def test_changed_camera(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][2]['source'].update(camera=1234))
    def test_changed_thread(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][2]['source'].update(thread=1234))
    def test_wrong_bound_source(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][1]['source'].update(data_sha256='0'*64))
    def test_wrong_sample(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][1].update(candidate=0))
    def test_changed_provider(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][1]['after'].update(sizing=1))
    def test_active_provider(self):
        def change(r):
            c=review.one(r,'stimulus')['calls'][1]
            c['before']['wrapper']=c['after']['wrapper']=1234
        self.changed(change)
    def test_missing_matrix_response(self):
        def change(r):
            for s in (review.one(r,'source'),review.one(r,'stimulus')['calls'][1]['source']):
                s['view_projection_bits']=s['unjittered_view_projection_bits'].copy()
        self.changed(change)
    def test_wrong_matrix_axis(self):
        def change(r):
            for s in (review.one(r,'source'),review.one(r,'stimulus')['calls'][1]['source']):
                s['view_projection_bits'][12],s['view_projection_bits'][13]=s['view_projection_bits'][13],s['view_projection_bits'][12]
        self.changed(change)
    def test_nonzero_recovery(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][2]['source']['jitter_bits'].__setitem__(0,0x3e800000))
    def test_publication_before_restore(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][1].update(exit_order=review.one(r,'publication')['order']))
    def test_timeout(self):
        self.changed(lambda r:review.one(r,'stimulus')['calls'][2]['source'].update(tick=2**62))
    def test_wrong_draw_viewport(self):
        self.changed(lambda r:next(a for a in r if a['event']=='api' and a['kind']==13)['viewport'].__setitem__(2,0x44f00000))
    def test_header_budget(self):
        rows=copy.deepcopy(self.rows);rows[0]['injection_budget']=2
        with self.assertRaises(ValueError):review.parse(('\n'.join(map(json.dumps,rows))+'\n').encode())
    def test_control_version(self):
        rows=copy.deepcopy(self.rows[:3]);rows[0].pop('owned_fixture')
        rows[0]['base']=review.one(rows,'trampoline_reservation')['origin']-0x228f3b0
        control.validate(rows,1,1,1)
        self.assertEqual(control.event_name(rows[0],1),'Local\\MhwProjectionBinding-v1-1-1-1')


if __name__=='__main__':
    sys.argv_fixture=sys.argv[1];sys.argv=sys.argv[:1];unittest.main()

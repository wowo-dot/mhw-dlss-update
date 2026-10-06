"""Review owned native attachment traces and reject independently damaged evidence."""
import copy
import json
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import review_projection_binding as review
import arm_projection_binding as control


class Attachments(unittest.TestCase):
    def rows(self,case='depth'):
        path=Path(sys.argv_fixture)/f'attach_{case}.jsonl'
        return review.parse(path.read_bytes()),lambda n:path.with_name(path.stem+f'-shader-{n}.bin').read_bytes()

    def changed(self,fn,case='depth'):
        rows,load=self.rows(case);fn(rows)
        with self.assertRaises(ValueError):
            review.review(review.parse(('\n'.join(json.dumps(r) for r in rows)+'\n').encode()),load)

    def draw(self,rows):return review.one(rows,'draw')
    def attachment(self,rows):return self.draw(rows)['attachments']
    def api(self,rows,kind):return next(r for r in rows if r['event']=='api' and r['kind']==kind)

    def test_depth(self):
        rows,load=self.rows();r=review.review(rows,load)
        self.assertTrue(r['attachment_resource_descriptions_correlated'])
        self.assertTrue(r['depth_write_candidate']);self.assertTrue(r['fresh_output_binding_observed'])
        self.assertEqual(r['recorded_barriers'],1)
        for k in ('actual_pipeline_creation_observed','created_pso_bytecode_correlated','executing_pso_bytecode_verified',
                  'view_descriptor_provenance_verified','resource_state_at_gpu_execution_verified','gpu_contents_verified','submission_verified','raster_jitter_verified'):
            self.assertFalse(r[k],k)

    def test_color_depth(self):
        rows,load=self.rows('color_depth');r=review.review(rows,load)
        self.assertEqual(r['attachment_color_count'],1);self.assertTrue(r['depth_resource_present'])
        self.assertEqual(len(self.attachment(rows)['resources']),2)

    def test_no_depth_ignores_stale_cache(self):
        rows,load=self.rows('no_depth');self.assertEqual(self.attachment(rows)['ready']['cached_depth_resource'],0xdeadbeef)
        r=review.review(rows,load);self.assertFalse(r['depth_resource_present']);self.assertIsNone(r['depth_resource_description'])
        self.assertFalse(r['depth_write_candidate'])

    def test_null_color(self):
        rows,load=self.rows('null_color');r=review.review(rows,load)
        self.assertEqual(r['attachment_color_count'],2);self.assertTrue(r['depth_write_candidate'])

    def test_cached(self):
        for case in ('cached','same_handles'):
            with self.subTest(case=case):
                rows,load=self.rows(case);r=review.review(rows,load)
                self.assertFalse(r['fresh_output_binding_observed']);self.assertFalse(r['actual_pipeline_creation_observed'])

    def test_created(self):
        rows,load=self.rows('created');r=review.review(rows,load)
        self.assertTrue(r['actual_pipeline_creation_observed']);self.assertTrue(r['created_pso_bytecode_correlated'])
        self.assertFalse(r['executing_pso_bytecode_verified']);self.assertFalse(r['raster_jitter_verified'])

    def test_adjusted_canonical_identity(self):
        rows,load=self.rows('identity_adjusted');r=review.review(rows,load)
        resource=self.attachment(rows)['resources'][0]
        self.assertNotEqual(resource['resource'],resource['identity'])
        self.assertTrue(r['resource_identity_roundtrip_verified']);self.assertEqual(r['resource_identity_checks'],3)

    def query(self,rows,phase=0):return self.attachment(rows)['resource_checks'][phase]['queries'][0]
    def test_identity_phase_missing(self):self.changed(lambda r:self.attachment(r)['resource_checks'].pop())
    def test_identity_phase_order(self):self.changed(lambda r:self.attachment(r)['resource_checks'][1].update(phase=0))
    def test_identity_query_missing(self):self.changed(lambda r:self.attachment(r)['resource_checks'][1]['queries'].clear())
    def test_identity_hresult(self):self.changed(lambda r:self.query(r)['results'].__setitem__(2,0x80004002))
    def test_identity_hresult_missing(self):self.changed(lambda r:self.query(r)['results'].pop())
    def test_identity_checkpoint(self):self.changed(lambda r:self.query(r).update(checkpoint=12))
    def test_identity_temporary_leak(self):self.changed(lambda r:self.query(r).update(temporaries_released=False))
    def test_identity_query_valid(self):self.changed(lambda r:self.query(r).update(valid=False))
    def test_identity_roundtrip(self):self.changed(lambda r:self.query(r).update(roundtrip=123))
    def test_canonical_self_mismatch(self):self.changed(lambda r:self.query(r).update(identity_repeat=123))
    def test_roundtrip_canonical_mismatch(self):self.changed(lambda r:self.query(r).update(roundtrip_identity=123))
    def test_identity_at_draw_changed(self):self.changed(lambda r:self.query(r,1).update(identity=123))
    def test_identity_at_return_changed(self):self.changed(lambda r:self.query(r,2).update(identity=123))
    def test_legacy_cannot_claim_adjusted_identity(self):self.changed(lambda r:r[0].update(version='0.3.0'),'identity_adjusted')

    def test_header(self):self.changed(lambda r:r[0].update(jitter_writes=1))
    def test_limits(self):self.changed(lambda r:r[0].update(max_barriers=65))
    def test_code_gates(self):self.changed(lambda r:review.one(r,'armed').update(live_spans=36))
    def test_interface_identity(self):self.changed(lambda r:self.attachment(r)['resources'][0].update(identity=123))
    def test_device(self):self.changed(lambda r:self.attachment(r)['resources'][0].update(device=123))
    def test_resource_extent(self):self.changed(lambda r:self.attachment(r)['resources'][0].update(width=0))
    def test_resource_dimension(self):self.changed(lambda r:self.attachment(r)['resources'][0].update(dimension=1))
    def test_duplicate_resource(self):self.changed(lambda r:self.attachment(r)['resources'].append(copy.deepcopy(self.attachment(r)['resources'][0])))
    def test_release(self):self.changed(lambda r:self.attachment(r).update(resources_released=False))
    def test_at_draw(self):self.changed(lambda r:self.attachment(r).update(at_draw=False))
    def test_return(self):self.changed(lambda r:self.api(r,20).update(returned=False))
    def test_rotation(self):self.changed(lambda r:self.attachment(r)['ready']['views'][8].update(rotating=1))
    def test_pointer_changed(self):self.changed(lambda r:self.attachment(r)['ready']['views'][8].update(resource=123))
    def test_native_state(self):self.changed(lambda r:self.attachment(r)['ready']['pipeline']['depth'].__setitem__(1,0))
    def test_stale_depth(self):self.changed(lambda r:self.attachment(r)['ready'].update(cached_depth_resource=123))
    def test_count(self):self.changed(lambda r:self.api(r,20)['outputs'].update(count=1))
    def test_wrong_om_handle(self):self.changed(lambda r:self.api(r,20)['outputs'].update(depth=123))
    def test_missing_om(self):self.changed(lambda r:r.remove(self.api(r,20)))
    def test_cached_missing_handle(self):self.changed(lambda r:self.attachment(r)['entry']['cached'].__setitem__(8,0),'cached')
    def test_reset(self):self.changed(lambda r:self.api(r,12).update(kind=18))
    def test_psobind(self):self.changed(lambda r:self.api(r,12)['args'].__setitem__(0,123))
    def test_viewport(self):self.changed(lambda r:self.api(r,13)['viewport'].__setitem__(2,0))
    def test_scissor(self):self.changed(lambda r:self.api(r,23)['scissor'].__setitem__(2,0))
    def test_partial_barrier(self):self.changed(lambda r:self.api(r,21)['barriers'][0].update(flags=1))
    def test_barrier_capacity(self):self.changed(lambda r:self.attachment(r).update(barrier_count=65))
    def test_barrier_count(self):self.changed(lambda r:self.api(r,21)['args'].__setitem__(0,2))
    def test_barrier_type(self):self.changed(lambda r:self.api(r,21)['barriers'][0].update(type=3))
    def test_draw_order(self):self.changed(lambda r:self.attachment(r).update(draw_order=1))
    def test_creation_hresult(self):self.changed(lambda r:self.draw(r)['pipeline_creation'].update(result=0x80004005),'created')
    def test_creation_missing_bytes(self):self.changed(lambda r:self.draw(r)['pipeline_creation'].update(bytecode_matches=False),'created')
    def test_creation_iid(self):self.changed(lambda r:self.draw(r)['pipeline_creation'].update(iid_valid=False),'created')
    def test_creation_pointer(self):self.changed(lambda r:self.draw(r)['pipeline_creation'].update(pso=123),'created')
    def test_creation_stability(self):self.changed(lambda r:self.draw(r)['pipeline_creation'].update(stable=False),'created')
    def test_creation_desc(self):
        def damage(r):
            p=self.draw(r)['pipeline_creation'];raw=bytearray.fromhex(p['descriptor_hex'])
            struct.pack_into('<I',raw,0x1f4,0);p['descriptor_hex']=raw.hex()
        self.changed(damage,'created')
    def test_creation_bytecode_size(self):self.changed(lambda r:self.draw(r)['pipeline_creation']['sizes'].__setitem__(0,1),'created')
    def test_cached_not_creation(self):self.changed(lambda r:self.draw(r)['pipeline_creation'].update(succeeded=True),'cached')
    def test_control_header(self):
        rows,_=self.rows();rows=copy.deepcopy(rows[:3]);rows[0].pop('owned_fixture')
        rows[0]['base']=review.one(rows,'trampoline_reservation')['origin']-0x228f3b0
        control.validate(rows,1,1,1)


if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()

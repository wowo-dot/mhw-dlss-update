"""Independent review/control tests against the native owned-memory fixture."""
import copy
import json
import math
import random
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import review_projection_binding as review
import arm_projection_binding as control
import projection_contract as contract

class Review(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = Path(sys.argv_fixture)
        cls.raw = (cls.directory/'chain.jsonl').read_bytes()
        cls.records = review.parse(cls.raw)

    def load(self, index):
        return (self.directory/f'chain-shader-{index}.bin').read_bytes()

    def reject(self, event, key, value):
        records = copy.deepcopy(self.records)
        review.one(records, event)[key] = value
        with self.assertRaises(ValueError):
            review.review(records, self.load)

    def test_chain(self):
        result = review.review(self.records, self.load)
        self.assertEqual(result['verdict'], 'root-cbv-call-chain-observed')
        self.assertEqual(result['root_parameters'], [2])
        self.assertFalse(result['shader_uses_projection_verified'])

    def test_cached(self):
        records = review.parse((self.directory/'cached.jsonl').read_bytes())
        result = review.review(records, lambda n: (self.directory/f'cached-shader-{n}.bin').read_bytes())
        self.assertEqual(result['verdict'], 'partial-binding-evidence')

    def test_pointer(self): self.reject('publication', 'pointer', 1234)
    def test_packet(self): self.reject('consumption', 'packet', 1234)
    def test_drop(self): self.reject('capture_complete', 'drops', 1)
    def test_pending(self): self.reject('capture_complete', 'pending', 1)
    def test_shutdown(self): self.reject('stopped', 'hooks_disabled', False)
    def test_changed(self): self.reject('draw', 'source_unchanged', False)
    def test_gpu_claim(self): self.reject('capture_complete', 'gpu_contents_verified', True)
    def test_corrupt_blob(self):
        with self.assertRaises(ValueError): review.review(self.records, lambda n: self.load(n)+b'x')
    def test_truncated(self):
        with self.assertRaises(ValueError): review.parse(self.raw[:-1])
    def test_versions(self):
        records = copy.deepcopy(self.records)
        records[0]['version'] = '0.1.0'
        review.parse(('\n'.join(json.dumps(r) for r in records)+'\n').encode())
        records[0]['version'] = '0.1.4'
        with self.assertRaises(ValueError): review.parse(('\n'.join(json.dumps(r) for r in records)+'\n').encode())
    def test_arena_overflow(self):
        records = copy.deepcopy(self.records)
        review.one(records, 'consumption')['arena']['cpu'][0] = 2**64-1
        with self.assertRaises(ValueError): review.review(records, self.load)
    def test_bind_after_draw(self):
        records = copy.deepcopy(self.records)
        bind = next(r for r in records if r['event']=='api' and r['kind']==9)
        draw = next(r for r in records if r['event']=='api' and r['kind']==14)
        bind['order'], draw['order'] = draw['order'], bind['order']
        bind['exit_order'], draw['exit_order'] = draw['exit_order'], bind['exit_order']
        # Evidence remains partial, even if a bind occurred later.
        result = review.review(records, self.load)
        self.assertEqual(result['verdict'], 'partial-binding-evidence')
    def test_root_signature_invalidates(self):
        records = copy.deepcopy(self.records)
        bind = next(r for r in records if r['event']=='api' and r['kind']==9)
        bind['kind'] = 11
        self.assertEqual(review.review(records, self.load)['verdict'], 'partial-binding-evidence')
    def test_control_fixture_rejected(self):
        with self.assertRaises(ValueError):control.validate(self.records,1,1,1)
    def test_control_ready(self):
        h = copy.deepcopy(self.records[0]);h.pop('owned_fixture')
        h['version'] = '0.1.1'
        records = [h,dict(event='ready',hooks_installed=False)]
        control.validate(records,1,1,1)
        self.assertEqual(control.event_name(h,1),'Local\\MhwProjectionBinding-v1-1-1-1')
        with self.assertRaises(ValueError):control.validate(records,1,1,2)
        with self.assertRaises(ValueError):control.validate(records+[dict(event='stopped')],1,1,1)
    def test_duplicate_armed(self):
        h = copy.deepcopy(self.records[0]);h.pop('owned_fixture')
        with self.assertRaises(ValueError):control.validate([h,dict(event='ready',hooks_installed=False),dict(event='armed')],1,1,1)
    def test_pool_missing(self):
        records = [r for r in self.records if r['event'] != 'trampoline_reservation']
        with self.assertRaises(ValueError):review.review(records, self.load)
    def test_pool_capacity(self): self.reject('trampoline_reservation', 'free_slots', 0)
    def test_pool_policy(self): self.reject('trampoline_reservation', 'policy_flags', 1)
    def test_pool_reachability(self): self.reject('trampoline_reservation', 'page', 4096)
    def test_control_pool(self):
        records = copy.deepcopy(self.records[:3]);records[0].pop('owned_fixture')
        pool = review.one(records, 'trampoline_reservation')
        records[0]['base'] = pool['origin']-0x228f3b0
        control.validate(records,1,1,1)
        pool['ready'] = False
        with self.assertRaises(ValueError):control.validate(records,1,1,1)
    def test_matrix_missing(self): self.reject('source','matrices_valid',False)
    def test_matrix_nonfinite(self):
        bits=review.one(self.records,'source')['view_projection_bits'].copy();bits[0]=0x7fc00000
        self.reject('source','view_projection_bits',bits)
    def test_matrix_wrong_product(self):
        bits=review.one(self.records,'source')['view_projection_bits'].copy();bits[0]=0x40000000
        self.reject('source','view_projection_bits',bits)
    def test_matrix_overlap(self):
        ids=review.one(self.records,'source')['matrix_ids'].copy();ids[1]=ids[0]
        self.reject('source','matrix_ids',ids)
    def test_root_mismatch(self):
        selection=copy.deepcopy(review.one(self.records,'draw')['root_selection']);selection['signatures'][selection['index']]+=1
        self.reject('draw','root_selection',selection)
    def test_root_changed(self):
        selection=copy.deepcopy(review.one(self.records,'draw')['root_selection']);selection['stable']=False
        self.reject('draw','root_selection',selection)
    def test_root_selector(self):
        selection=copy.deepcopy(review.one(self.records,'draw')['root_selection']);selection['ps_count']=1
        self.reject('draw','root_selection',selection)
    def test_root_no_signature_call(self):
        result=review.review(self.records,self.load)
        self.assertFalse(result['actual_root_signature_call_observed'])
        self.assertFalse(result['native_metadata_vs_b0_binding_correlated'])
    def test_root_positive_and_wrong_signature(self):
        d=copy.deepcopy(review.one(self.records,'draw'))
        shader=[dict(sha256=contract.KNOWN_VERTEX)]
        s=review.one(self.records,'source')
        apis=[dict(kind=11,args=[d['root_cache']],order=1),dict(kind=9,args=[2,d['gpu']],order=2),dict(kind=14,args=[],order=3)]
        self.assertTrue(contract.root_layout(d,apis,[2],shader,s)['native_metadata_vs_b0_binding_correlated'])
        apis[0]['args'][0]+=1
        self.assertFalse(contract.root_layout(d,apis,[2],shader,s)['native_metadata_vs_b0_binding_correlated'])
    def test_root_unknown_shader_and_descriptor(self):
        d=copy.deepcopy(review.one(self.records,'draw'));s=review.one(self.records,'source')
        apis=[dict(kind=11,args=[d['root_cache']],order=1),dict(kind=9,args=[2,d['gpu']],order=2),dict(kind=14,args=[],order=3)]
        self.assertFalse(contract.root_layout(d,apis,[2],[dict(sha256='unknown')],s)['native_metadata_vs_b0_binding_correlated'])
        d['binding_mode']=1;d['root_selection']['descriptor_mode']=1;d['root_selection']['recipe_flags'][2]=1
        self.assertFalse(contract.root_layout(d,apis,[2],[dict(sha256=contract.KNOWN_VERTEX)],s)['native_metadata_vs_b0_binding_correlated'])
    def test_matrix_asymmetric_nonzero_jitter(self):
        f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
        bits=lambda a:list(struct.unpack('<'+'I'*len(a),struct.pack('<'+'f'*len(a),*a)))
        def product(a,b):
            return [f32(f32(f32(f32(a[4*r]*b[c])+f32(a[4*r+1]*b[4+c]))+f32(a[4*r+2]*b[8+c]))+f32(a[4*r+3]*b[12+c])) for r in range(4) for c in range(4)]
        rng=random.Random(6444)
        identity=[float(r==c) for r in range(4) for c in range(4)]
        depth=identity.copy();depth[10]=-1.;depth[14]=1.
        for _ in range(128):
            angle=rng.uniform(-math.pi,math.pi);co=f32(math.cos(angle));si=f32(math.sin(angle))
            v=[co,0.,si,0.,0.,1.,0.,0.,-si,0.,co,0.,f32(rng.uniform(-100,100)),f32(rng.uniform(-100,100)),f32(rng.uniform(-100,100)),1.]
            p=list(map(f32,[1.3,0,0,0,0,2.1,0,0,.03,-.02,.9,-1,0,0,14,0]))
            jx,jy=map(f32,(rng.uniform(-.5,.5),rng.uniform(-.5,.5)))
            j=identity.copy();j[12]=f32(jx/2560);j[13]=f32(jy/1440)
            s=copy.deepcopy(review.one(self.records,'source'))
            pj=product(p,j);s['view_bits']=bits(v);s['jitter_bits']=bits([jx,jy])
            s['projection_bits']=bits(product(pj,depth));s['unjittered_bits']=bits(product(p,depth))
            s['view_projection_bits']=bits(product(product(v,pj),depth));s['unjittered_view_projection_bits']=bits(product(product(v,p),depth))
            self.assertTrue(contract.matrices(s)['nonzero_jitter_observed'])
        transposed=[v[c*4+r] for r in range(4) for c in range(4)];s['view_bits']=bits(transposed)
        with self.assertRaises(ValueError):contract.matrices(s)

if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()

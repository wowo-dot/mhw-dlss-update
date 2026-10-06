"""Replay owned native projection fixtures, including scaled denominator failures."""
import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_jitter import review_jitter
from review_scaled_temporal import parse

class ScaledProjectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not hasattr(sys,'fixture_root'):raise unittest.SkipTest('owned projection fixtures required')
        cls.folder=Path(sys.fixture_root)
        cls.header=json.loads((cls.folder/'header.json').read_text())
        cls.header.update(version='1.23.5',hooks=22,jitter_writes=1,scaled_projection=True,projection_activation_scope=True,
                          projection_samples=3,projection_byte_limit=4096,jitter_restorations=1,
                          jitter_min_abs=.3125,jitter_selection_limit=20)

    def fixture(self,basis):
        j=json.loads((self.folder/f'jitter-{basis}.json').read_text())
        r=j['records'][0];h=dict(self.header,base=r['caller']-0x228e834)
        c=dict(width=2560,height=1440,renderer=r['renderer'],backend=r['backend'],device=r['device'],
               started_tick=r['enter_tick'],projection_stimulus=j,records=[])
        for p in j['records']:
            cpu={k:p[k] for k in ('frame','epoch','bank','async','context','view')}
            cpu.update(enter_tick=p['exit_tick'],exit_tick=p['exit_tick'],rectangle=[0,0,1280,720],
                       scale_state=dict(stable=True,frame=p['frame'],view=p['view'],display=[2560,1440],render=[1280,720],active=.5,content=1))
            c['records'].append(cpu)
        return h,c

    def test_measured_projection_bases(self):
        for basis,shift in [('render',[.1875,.21875]),('allocation',[.09375,.109375])]:
            h,c=self.fixture(basis)
            result=review_jitter(h,c)
            self.assertEqual(result['projection_samples'][1]['predicted_raster_shift'],shift)
            self.assertEqual(result['projection_samples'][0]['predicted_raster_shift'],[0,0])
            self.assertEqual(result['projection_samples'][2]['predicted_raster_shift'],[0,0])

    def test_association_damage(self):
        h,c=self.fixture('allocation')
        for key,value in [('stable',False),('frame',999),('view',1),('display',[1280,720]),('render',[2560,1440]),('content',.5),('active',1)]:
            bad=copy.deepcopy(c);bad['records'][1]['scale_state'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review_jitter(h,bad)
        for key in ('frame','epoch','bank','async','context','view'):
            bad=copy.deepcopy(c);bad['records'][1][key]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):review_jitter(h,bad)

    def test_matrix_and_restoration_damage(self):
        h,c=self.fixture('render')
        for key,value in [('restored',False),('restored_bits',1),('jitter_bits',[0,0]),('viewport_bits',[0,0]),('projection_bits',[0]*16)]:
            bad=copy.deepcopy(c);bad['projection_stimulus']['records'][1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review_jitter(h,bad)

    def test_projection_budgets(self):
        self.assertEqual(parse(json.dumps(self.header).encode())[0],self.header)
        for key,value in [('projection_activation_scope',False),('hooks',21),('jitter_writes',0),('scaled_projection',False),('projection_samples',4),
                          ('projection_byte_limit',8192),('jitter_restorations',0),('jitter_min_abs',.1),('jitter_selection_limit',21),('sdk_evaluation_limit',1)]:
            bad=dict(self.header);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):parse(json.dumps(bad).encode())

    def test_frame_scope_damage(self):
        h,c=self.fixture('allocation')
        for key,value in [('frame_scope_required',False),('frame_scope_waits',-1),('frame_scope_waits',True)]:
            bad=copy.deepcopy(c);bad['projection_stimulus'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review_jitter(h,bad)
        for index in range(3):
            bad=copy.deepcopy(c);bad['projection_stimulus']['records'][index]['frame_scope_admitted']=False
            with self.subTest(index=index),self.assertRaises(ValueError):review_jitter(h,bad)

if __name__=='__main__':
    if len(sys.argv)>1 and not sys.argv[1].startswith('-'):sys.fixture_root=sys.argv.pop(1)
    unittest.main()

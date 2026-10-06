from copy import deepcopy
import json
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_scaled import review
from arm_continuous_dlaa import parse,HOST

def sample():
    r=dict(schema=1,fault=0,pending=0,hooks_disabled=True,published=105,built=104,unqualified=1,claimed=100,copies=100,draws=300,complete=100,warmup_draws=12,retained=100,released=100,live=0,peak=3,active=False,failure='')
    p=dict(mode=0,preset=11,captures=0,geometry_schema=1,allocation=[2560,1440],render=[1280,720],output_extent=[2560,1440],optimal=[1280,720],input_origin=[0,0],motion_units='render_pixels',jitter_units='render_pixels')
    return dict(frames=100,width=2560,height=1440,render_width=1280,render_height=720,scaled_route=r,sdk=p,resolution=dict(fault=0,shared_viewport=True,render=[1280,720],display=[2560,1440],requested=False,restored=True,hooks_disabled=True,pending=0))
class ScaledTests(unittest.TestCase):
    def test_prepared_multilist(self):
        s=sample();s['frames']=95;r=s['scaled_route']
        r.update(schema=3,prepared_frames=100,spatial_frames=3,native_frames=2,early_draws=80,multilist_frames=40,submitted_frames=100,
                 unresolved_frames=0,unsubmitted_frames=0,selection='immutable-frame-predicate-v1',retained=180,released=180,peak=8,
                 haze_claimed=0,haze_complete=0,haze_draws=0,haze_admission_rejection=0)
        self.assertEqual(review(s,final=True)['complete'],100)
        for key,value in [('prepared_frames',99),('spatial_frames',2),('native_frames',3),('submitted_frames',99),('unresolved_frames',1),('unsubmitted_frames',1),('early_draws',301),('multilist_frames',101),('selection','mutable')]:
            bad=deepcopy(s);bad['scaled_route'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(bad,final=True)
        old=review(s);r['early_draws']-=1
        with self.assertRaises(ValueError):review(s,old)
    def test_completed(self): self.assertEqual(review(sample(),final=True)['complete'],100)
    def test_final_quad_provenance(self):
        s=sample();s['scaled_route'].update(final_captured=105,final_rejected=0)
        self.assertEqual(review(s,final=True)['final_captured'],105)
        s['scaled_route']['final_captured']=103
        with self.assertRaises(ValueError):review(s)
    def test_missing_effect_or_copy(self):
        for key,value in [('copies',99),('draws',299),('complete',99),('claimed',101),('fault',1),('pending',1),('active',True),('hooks_disabled',False),('released',101)]:
            s=sample();s['scaled_route'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(s,final=True)
    def test_scale_restore(self):
        for key,value in [('restored',False),('requested',True),('hooks_disabled',False),('pending',1),('fault',1),('shared_viewport',False),('render',[2560,1440])]:
            s=sample();s['resolution'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(s,final=True)
    def test_sdk_geometry(self):
        for key,value in [('mode',5),('captures',1),('optimal',[2560,1440]),('render',[2560,1440]),('input_origin',[1,0]),('motion_units','output_pixels')]:
            s=sample();s['sdk'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(s)
    def test_inflight(self):
        s=sample();r=s['scaled_route'];r.update(claimed=101,retained=101,live=1,active=True,hooks_disabled=False);self.assertEqual(review(s)['live'],1)
        r['claimed']=102;r['retained']=102;r['live']=2
        with self.assertRaises(ValueError):review(s)
    def test_rollback(self):
        old=review(sample());s=sample();s['scaled_route']['published']-=1
        with self.assertRaises(ValueError):review(s,old)
    def test_optional_haze_chain(self):
        s=sample();r=s['scaled_route']
        r.update(schema=2,haze_claimed=40,haze_complete=40,haze_draws=40,haze_admission_rejection=0,draws=340)
        self.assertEqual(review(s,final=True)['haze_complete'],40)
        for key,value in [('draws',339),('haze_complete',39),('haze_claimed',41),('haze_draws',39),('haze_admission_rejection',7)]:
            bad=deepcopy(s);bad['scaled_route'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(bad,final=True)
        for key in ('haze_claimed','haze_complete','haze_draws'):
            bad=deepcopy(s);del bad['scaled_route'][key]
            with self.assertRaises(ValueError):review(bad)
    def test_haze_inflight(self):
        s=sample();r=s['scaled_route']
        r.update(schema=2,haze_claimed=1,haze_complete=0,haze_draws=0,haze_admission_rejection=0,
                 claimed=101,retained=101,live=1,active=True,hooks_disabled=False)
        review(s);r.update(draws=302,haze_draws=1,copies=101);review(s)
        r['draws']=305
        with self.assertRaises(ValueError):review(s)
    def test_header(self):
        h=dict(event='start',schema=1,version='1.25.0',host_sha256=HOST,host_version='0.0.0.0',experiment='continuous-scaled-native-resolve',replacement_scope='qualified-native-resolve',exclusive_mode=False,retention='reset-and-fence-v1',request_retirement='validated-native-return-v1',activation='native-postqueue-preparation-v2',frame_limit=None,graphics_api='D3D12',reconstruction='suspend-or-latch-unfiltered-v1',pid=1,creation=2,base=3,state_trace='consumer-local-state-v1',native_state_trace='scheduled-state-evidence-v1',state_admission='native-current-use-v1',copy_telemetry='single-fence-observation-v1',depth_eligibility='depth-eligibility-observation-v1',depth_destination='depth-destination-identity-v1')
        self.assertEqual(parse(json.dumps(h).encode())[0],h)
        for version in ('1.25.1','1.25.2','1.25.3','1.25.4', '1.25.5', '1.25.6','1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24','1.25.25', '1.25.26', '1.25.27'):
            h['version']=version;self.assertEqual(parse(json.dumps(h).encode())[0],h)
        h['experiment']='native-temporal-resolve'
        with self.assertRaises(ValueError):parse(json.dumps(h).encode())
        h['experiment']='continuous-scaled-native-resolve';h['version']='1.25.43'
        self.assertEqual(parse(json.dumps(h).encode())[0],h)
if __name__=='__main__':unittest.main()

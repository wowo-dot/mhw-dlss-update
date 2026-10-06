import copy
import json
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from review_jitter_activation import HOST_SHA,parse,review
from arm_jitter_activation import event_name,validate

def bits(f): return struct.unpack('<I',struct.pack('<f',f))[0]

def fixture():
    base=0x140000000
    rows=[dict(event='start',schema=1,version='1.0.0',host_version='0.0.0.0',host_sha256=HOST_SHA,
               experiment='scoped-camera-jitter',provider_substitution=False,record_capacity=3,window_ms=1500,visit_limit=256,
               pid=42,creation=99,base=base,dll_sha256='a'*64),
          dict(event='ready',hooks_installed=False),
          dict(event='adapter_inventory',gpu_instrumented=False,active_adapter_identified=False,swap_chain_format=None),
          dict(event='armed',live_spans=8,d3d12_module_loaded=True,graphics_api_confirmed=False,tick=100)]
    for i in range(3):
        j=[bits(.25),bits(-.25)]; n=[0.]*16
        for k in range(4): n[4*k+k]=1.
        p=n.copy()
        if i==1: p[12]=.25/2560; p[13]=-.25/1440
        rows.append(dict(event='sample',stage=i,thread=5,tick=110+20*i,exit_tick=112+20*i,rejection=0,
            view_flags=0,projection_active=1,display_index=0,use_dlss=0,projection_object=0x200000050,
            camera=0x200000000,camera_type=base+0x350d188,caller=base+0x228e834,original_jitter=0,
            candidate=j[0]+(j[1]<<32),restored_jitter=0,injected=i==1,restored=True,returned=True,result=True,complete=True,
            anchor=dict(renderer=0x300000000,context=0x300000710,view=0x400000000,table=0x500000000,frame=10+i,epoch=10+i,bank=i%2,async_mode=0),
            projection=dict(valid=True,matrix_relation=True,rejection=0,checkpoint=100,reads=110,epoch=10+i,
                camera=0x200000000,vtable_rva=0x350d188,buffer_id=0,projection_id=0xf00,unjittered_id=0x10f00,words=64,constants=0x600000000,
                jitter_bits=j if i==1 else [0,0],table20_bits=j,viewport_bits=[bits(2560),bits(1440)],projection_bits=list(map(bits,p)),unjittered_bits=list(map(bits,n)))))
    rows += [dict(event='capture_complete',records=3,reserved=3,pending=0,stop=2,injections=1,restorations=1,dropped=0,drained=True,visits=3),
             dict(event='stopped',fatal=False,hooks_disabled=True,pending=0,dll_pinned=True,trampolines_retained=True)]
    return rows

def raw(rows): return ('\n'.join(map(json.dumps,rows))+'\n').encode()

def corrected_fixture():
    rows=fixture();rows[0]['version']='1.0.2';rows[3]['live_spans']=9
    rows[3]['installation']=dict(stage=3,status=0,status_name='MH_OK',hook_created=True,hook_enabled=True)
    for r in rows[4:7]:
        r.pop('use_dlss');r['admission_checkpoint']=100;r['camera_stamp']=r['anchor']['epoch']
        r['provider_before']=dict(valid=True,backend=0x700000000,wrapper=0,use_dlss=1,sizing_flags=0)
        r['provider_after']=r['provider_before'].copy()
    return rows

class ActivationTests(unittest.TestCase):
    def test_corrected_raw_switch(self):
        self.assertTrue(review(parse(raw(corrected_fixture())))['accepted_cpu_experiment'])
    def test_corrected_provider_exclusion(self):
        for side in ('provider_before','provider_after'):
            for key,value in dict(valid=False,wrapper=123,sizing_flags=1,backend=0,use_dlss=2).items():
                rows=corrected_fixture();rows[5][side][key]=value
                self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_corrected_provider_change(self):
        rows=corrected_fixture();rows[5]['provider_after']['backend']+=16
        self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_corrected_admission_checkpoint(self):
        rows=corrected_fixture();rows[5]['admission_checkpoint']=6
        self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_complete(self):
        result=review(parse(raw(fixture())))
        self.assertTrue(result['accepted_cpu_experiment']); self.assertFalse(result['gpu_qualified'])
        self.assertGreater(result['projection_delta_max'][1],0)
    def test_installation_diagnostics(self):
        rows=fixture(); rows[0]['version']='1.0.1'
        self.assertFalse(review(rows)['accepted_cpu_experiment'])
        rows[3]['installation']=dict(stage=3,status=0,status_name='MH_OK',hook_created=True,hook_enabled=True)
        self.assertTrue(review(parse(raw(rows)))['accepted_cpu_experiment'])
        rows[3]['installation']['hook_enabled']=False
        self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_failure_details_retained(self):
        rows=fixture()[:2]+[dict(event='failed',reason='hook-install',installation=dict(stage=2,status=8))]
        result=review(parse(raw(rows)))
        self.assertFalse(result['accepted_cpu_experiment']);self.assertEqual(result['failed_events'][0]['installation']['stage'],2)
    def test_missing_or_empty_capture(self):
        for index in (4,5,6,7,8):
            rows=fixture(); rows.pop(index); self.assertFalse(review(parse(raw(rows)))['accepted_cpu_experiment'])
        self.assertFalse(review(parse(raw(fixture()[:2])))['accepted_cpu_experiment'])
    def test_bad_completion(self):
        for key,value in dict(pending=1,restorations=0,injections=2,stop=5,dropped=1,drained=False,visits=257).items():
            with self.subTest(key=key):
                rows=fixture(); rows[7][key]=value; self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_camera_restore_failure(self):
        for key,value in dict(restored=False,restored_jitter=123,rejection=6,returned=False,complete=False).items():
            rows=fixture(); rows[5][key]=value; self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_changed_anchor(self):
        for key in ('renderer','view','table','context'):
            rows=fixture(); rows[5]['anchor'][key]+=8; self.assertFalse(review(rows)['accepted_cpu_experiment'])
        rows=fixture(); rows[5]['projection']['camera']+=8; self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_wrong_scale(self):
        rows=fixture(); rows[5]['projection']['projection_bits'][12]=bits(.5/2560)
        self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_nonfinite(self):
        for key in ('projection_bits','unjittered_bits','viewport_bits'):
            rows=fixture(); rows[5]['projection'][key][0]=0x7fc00000; self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_no_effect(self):
        rows=fixture(); rows[5]['projection']['projection_bits']=copy.copy(rows[5]['projection']['unjittered_bits'])
        self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_phase_order(self):
        for key in ('frame','epoch'):
            rows=fixture(); rows[5]['anchor'][key]=10; self.assertFalse(review(rows)['accepted_cpu_experiment'])
        rows=fixture(); rows[4],rows[5]=rows[5],rows[4]; self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_control_generation(self):
        rows=fixture()[:2]; validate(rows,1,42,99)
        self.assertEqual(event_name(rows[0],1),'Local\\MhwJitterActivation-v1-42-99-1')
        with self.assertRaises(ValueError): validate(rows,1,42,100)
        with self.assertRaises(ValueError): validate(fixture(),1,42,99)
    def test_legacy_rejected(self):
        for key,value in dict(schema=2,experiment='typed-jitter-property-writer',window_ms=180000,provider_substitution=True).items():
            rows=fixture(); rows[0][key]=value
            with self.assertRaises(ValueError): parse(raw(rows))
    def test_malformed(self):
        data=raw(fixture())
        for bad in (data[:-1],data.replace(b'"stage": 0',b'"stage": 0, "stage": 0'),data.replace(b'"stage": 0',b'"stage": NaN')):
            with self.assertRaises(ValueError): parse(bad)
    def test_layout(self):
        for key,value in dict(projection_id=0xffffffff,unjittered_id=0xf00,words=8,constants=3).items():
            rows=fixture(); rows[5]['projection'][key]=value; self.assertFalse(review(rows)['accepted_cpu_experiment'])
    def test_time_limit(self):
        rows=fixture(); rows[6]['tick']=1600; rows[6]['exit_tick']=1601
        self.assertFalse(review(rows)['accepted_cpu_experiment'])

if __name__=='__main__': unittest.main()


"""Damage actual owned GPU triplets; CPU/core composition is explicitly synthetic."""
import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import projection_alignment as alignment
import projection_contract as contract
import projection_readback as readback
import projection_triplet as triplet
import review_projection_binding as review
import test_projection_readback as fixtures

root=Path(sys.argv.pop(1)) if len(sys.argv)>1 else Path('local/projection-binding-20260923/v0.8.4/fixtures')
def bits(values):return list(struct.unpack('<'+'I'*len(values),struct.pack('<'+'f'*len(values),*values)))
def source(selection):
    s=dict(selection);s['order']=s.pop('core_order');s.pop('published')
    s['viewport_bits']=bits([16,16]);s['jitter_bits']=bits([.25,-.25])
    return s
def controls(path,stages=(0,2)):
    rows=[];raw={}
    for stage in stages:
        d=json.loads(Path(str(path)+f'.depth.f32.control-{stage}.json').read_text())
        f=Path(str(path)+f'.depth.f32.control-{stage}.f32')
        raw[stage]=f.read_bytes() if f.exists() else b''
        rows.append(dict(event='depth_control',stage=stage,saved=d['phase']==7,
            file=f'binding-depth-control-{stage}.f32',sha256=hashlib.sha256(raw[stage]).hexdigest(),details=d))
    return rows,lambda stage,record:raw[stage]

class Triplet(unittest.TestCase):
    def data(self,case='pixels'):
        name='readback_triplet_'+case;f=fixtures.Readback();f.fixture_root=root
        t,r,main,raw=f.data(name)
        r.update(readback.validate(t,r,main,raw,True))
        selected=source(t['frame_packets']['selection'])
        r.update(alignment.validate(t,r,selected,main['details']))
        r['stimulus_cpu_sequence_verified']=True # Unit boundary; full test below checks CPU evidence.
        path=root/(name+'.json');extra,loader=controls(path)
        choices=t['frame_packets']['controls'];sources=[source(choices[0]['selection']),selected,source(choices[1]['selection'])]
        rows=[main,json.loads(Path(str(path)+'.ready.jsonl').read_text()),
              dict(event='stimulus',calls=[dict(source=s) for s in sources]),*extra]
        return rows,t,r,loader
    def setUp(self):self.rows,self.t,self.r,self.loader=self.data()
    def valid(self):return triplet.validate(self.rows,self.t,self.r,self.loader)
    def reject(self,fn):fn();self.assertRaises((ValueError,KeyError,StopIteration),self.valid)
    @property
    def control(self):return self.rows[-2]
    @property
    def selection(self):return self.t['frame_packets']['controls'][0]['selection']
    def test_owned_gpu_values_and_scope(self):
        result=self.valid();self.assertTrue(result['depth_triplet_request_stamps_verified'])
        self.assertEqual(result['depth_triplet_frames'],[101,102,103])
        self.assertEqual(result['expected_stimulus_pixel_displacement'],[.125,.125])
        self.assertEqual([r['depth_pixel_statistics']['minimum'] for r in result['depth_control_results']],[.25,.5])
        self.assertFalse(result['depth_control_draw_bindings_verified'] or result['raster_jitter_verified'])
    def test_bad_native_controls(self):
        for case in ('missing_baseline','wrong_recovery','view_changed','extra_copy'):
            with self.subTest(case=case),self.assertRaises((ValueError,KeyError,StopIteration)):
                triplet.validate(*self.data(case))
    def test_missing_loader(self):self.assertRaises(ValueError,triplet.validate,self.rows,self.t,self.r,None)
    def test_missing_control(self):self.reject(lambda:self.rows.pop())
    def test_duplicate_stage(self):self.reject(lambda:self.rows[-1].update(stage=0))
    def test_bool_stage(self):self.reject(lambda:self.control.update(stage=False))
    def test_reversed_controls(self):self.reject(lambda:self.rows.__setitem__(slice(-2,None),self.rows[-2:][::-1]))
    def test_missing_selection(self):self.reject(lambda:self.t['frame_packets']['controls'].pop())
    def test_selection_frame(self):self.reject(lambda:self.selection.update(frame=100))
    def test_selection_epoch(self):self.reject(lambda:self.selection.update(epoch=90))
    def test_selection_thread(self):self.reject(lambda:self.selection.update(thread=1234))
    def test_selection_constants(self):self.reject(lambda:self.selection.update(constants=1234))
    def test_selection_pending(self):self.reject(lambda:self.selection.update(published=1))
    def test_site_selection(self):self.reject(lambda:self.control['details']['site'].update(selection=999999))
    def test_request_control_required(self):self.reject(lambda:self.control['details']['site'].pop('request_control'))
    def test_no_fabricated_draw(self):self.reject(lambda:self.control['details']['site'].update(marker=self.t['marker']['order']))
    def test_pixel_hash(self):self.reject(lambda:self.control.update(sha256='0'*64))
    def test_recording_unretired(self):self.reject(lambda:self.control['details'].update(recording_retired=False))
    def test_incomplete_fence(self):self.reject(lambda:self.control['details'].update(completed_value=0))
    def test_private_buffer_reuse(self):self.reject(lambda:self.control['details'].update(buffer=self.rows[0]['details']['buffer']))
    def test_private_fence_reuse(self):self.reject(lambda:self.control['details'].update(fence=self.rows[0]['details']['fence']))
    def test_added_order_collision(self):self.reject(lambda:self.control['details']['site'].update(begin=self.control['details']['site']['copy_exit']))
    def test_unverified_stimulus(self):self.reject(lambda:self.r.update(stimulus_cpu_sequence_verified=False))
    def test_wrong_resource(self):self.reject(lambda:self.control['details']['site'].update(resource=1234))
    def test_wrong_queue(self):self.reject(lambda:self.control['details']['site'].update(queue=1234))
    def test_request_consumed_twice(self):
        def edit():
            selected=next(c for c in self.t['frame_packets']['consumers'] if c['copy']==self.control['details']['site']['copy'])
            other=next(c for c in self.t['frame_packets']['consumers'] if c is not selected)
            other['receipt']=selected['receipt']
        self.reject(edit)
    def test_copy_packet_before_control_source(self):
        self.reject(lambda:self.selection.update(order=self.t['marker']['order']))
    def test_recovery_is_stimulus_copy(self):
        self.reject(lambda:self.rows[-1].update(details=copy.deepcopy(self.rows[0]['details'])))
    def test_control_not_accepted_as_main_draw(self):
        self.assertRaises(ValueError,readback.validate,self.t,self.r,self.control,self.loader(0,self.control),True)
    def test_descriptor_reconstruction(self):
        def edit():
            e=copy.deepcopy(next(e for e in self.t['watch'] if e['kind']==0))
            e.update(enter=self.control['details']['site']['copy']-2,exit=self.control['details']['site']['copy']-1)
            self.t['watch'].append(e)
        self.reject(edit)
    def test_new_selection_collision(self):
        self.reject(lambda:self.selection.update(order=self.t['frame_packets']['selection']['order']))
    def full(self,case="readback_triplet_pixels",count=3):
        # Independent core and GPU fixtures composed to exercise the complete
        # review API, never represented as an actual game capture.
        f=fixtures.Readback();f.fixture_root=root
        rows,blobs,t,pixels=f.combined(case)
        path=root/(case+'.json');original=json.loads(path.read_text())
        extra,loader=controls(path,[i for i in range(count) if i!=1]);s=review.one(rows,'source');h=rows[0]
        h.update(version=('0.9.1' if review.one(rows,'provenance_ready')['history_capacity']==8192 else '0.9.0') if count==5 else '0.8.4',experiment='projection-jitter-depth-sequence' if count==5 else 'projection-jitter-depth-triplet',jitter_writes=count-2,
            injection_budget=count-2,cpu_samples=count,max_recovery_deferrals=64,gpu_copy_budget=count,queue_signal_budget=count)
        if count==5:h['max_draw_waits']=64
        for row in rows:
            for key in ('order','exit_order'):
                if key in row:row[key]+=100
        for key in ('state_order','draw_order'):
            review.one(rows,'draw')['attachments'][key]+=100
        choices=[t['frame_packets']['controls'][0]['selection'],t['frame_packets']['selection'],*[c['selection'] for c in t['frame_packets']['controls'][1:]]]
        # Keep real owned GPU orders/stamps; remap their independent addresses.
        remap={original['marker'][k]:t['marker'][k] for k in ('resource','identity','command')}
        for r in extra:
            for key in ('resource','identity','command'):
                r['details']['site'][key]=remap.get(r['details']['site'][key],r['details']['site'][key])
        plain=copy.deepcopy(s);calls=[];candidate=bits([.25,-.25]);armed=review.one(rows,'armed')
        provider=dict(valid=True,use=0,sizing=0,wrapper=0,backend=armed['backend'],device=armed['device'])
        for stage,choice in enumerate(choices):
            v=copy.deepcopy(plain)
            for key in ('frame','epoch','bank','async_mode','thread'):v[key]=choice[key]
            xy=([.25,-.25],[-.25,.375],[.125,-.125])[stage-1] if 0<stage<count-1 else [.25,-.25]
            candidate=bits(xy)
            v.update(order=1+100*stage,jitter_bits=candidate if 0<stage<count-1 else [0,0],table20_bits=candidate)
            for key in ('projection_bits','view_projection_bits'):
                basekey='unjittered_bits' if key=='projection_bits' else 'unjittered_view_projection_bits'
                matrix=list(contract.floats(v[basekey],16));width,height=contract.floats(v['viewport_bits'],2)
                if 0<stage<count-1:
                    for row in range(4):matrix[4*row]+=xy[0]/width*matrix[4*row+3];matrix[4*row+1]+=xy[1]/height*matrix[4*row+3]
                v[key]=bits(matrix)
            for key,original_key in [('core_order','order'),('renderer','renderer'),('context','context'),('constants','constants'),('buffer_id','buffer_id')]:choice[key]=v[original_key]
            calls.append(dict(stage=stage,source=v,candidate=candidate[0]+(candidate[1]<<32),before=provider,after=provider,
                injected=0<stage<count-1,restored=True,restored_bits=0,returned=True,complete=True,exit_order=v['order']+1))
        s.update(calls[1]['source'])
        rows.extend([dict(event='stimulus',count=count,injections=count-2,restorations=count-2,recovery_deferrals=0,draw_waits=0,calls=calls),*extra])
        rows=review.parse(('\n'.join(map(json.dumps,rows))+'\n').encode())
        return rows,blobs,t,pixels,loader
    def test_full_review_composed_owned_traces(self):
        result=review.review(*self.full())
        self.assertTrue(result['stimulus_cpu_sequence_verified'] and result['depth_triplet_readbacks_completed'])
        self.assertFalse(result['raster_jitter_verified'])
    def test_previous_triplet_version_compatible(self):
        args=list(self.full());args[0][0]['version']='0.8.3'
        args[0]=review.parse(('\n'.join(map(json.dumps,args[0]))+'\n').encode())
        result=review.review(*args)
        self.assertTrue(result['stimulus_cpu_sequence_verified'] and result['depth_triplet_readbacks_completed'])
        self.assertFalse(result['raster_jitter_verified'])
    def test_full_review_control_required(self):
        args=list(self.full());args[-1]=None
        self.assertRaises(ValueError,review.review,*args)
    def test_header_budgets(self):
        rows,*_=self.full()
        for field,value in [('injection_budget',2),('gpu_copy_budget',1),('queue_signal_budget',4),('jitter_writes',0),('cpu_samples',2)]:
            bad=copy.deepcopy(rows);bad[0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review.parse(('\n'.join(map(json.dumps,bad))+'\n').encode())
    def test_full_review_cpu_restore_required(self):
        args=list(self.full());review.one(args[0],'stimulus')['calls'][1]['restored']=False
        self.assertRaises(ValueError,review.review,*args)
    def test_full_review_triplet_readiness_required(self):
        args=list(self.full());review.one(args[0],'provenance_ready')['triplet_readback']=False
        self.assertRaises(ValueError,review.review,*args)
    def test_full_review_predicted_pixel_shift(self):
        args=self.full();r=review.review(*args);s=review.one(args[0],'source')
        proj=contract.floats(s['projection_bits'],16);plain=contract.floats(s['unjittered_bits'],16)
        width,height=contract.floats(s['viewport_bits'],2)
        for point in ([.3,-.4,2.,1.],[-1.,.2,10.,1.]):
            a=[sum(point[k]*plain[4*k+c] for k in range(4)) for c in range(4)]
            b=[sum(point[k]*proj[4*k+c] for k in range(4)) for c in range(4)]
            self.assertAlmostEqual((b[0]/b[3]-a[0]/a[3])*width/2,r['expected_stimulus_pixel_displacement'][0],places=6)
            self.assertAlmostEqual(-(b[1]/b[3]-a[1]/a[3])*height/2,r['expected_stimulus_pixel_displacement'][1],places=6)
    def test_full_review_old_version_cannot_claim_triplet(self):
        args=list(self.full());args[0][0]['version']='0.8.2'
        self.assertRaises(ValueError,review.review,*args)

if __name__=='__main__':unittest.main()

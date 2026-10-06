import copy
import json
from pathlib import Path
import sys
import unittest
import hashlib
import shutil
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import review_scaled_temporal as scaled
from arm_temporal_inputs import validate

class ScaledTemporalTests(unittest.TestCase):
    def test_color_admission_binding(self):
        if not hasattr(sys,'argv_fixture'):self.skipTest('retained WARP fixture required')
        c=json.loads(Path(sys.argv_fixture).read_text())
        if not c['native_compute']['records'][0].get('color_admission',{}).get('attempted'):
            self.skipTest('earlier-transition fixture required')
        from temporal_color_state import review
        for cpu,native,s in zip(c['records'],c['native_compute']['records'],c['submission']['samples']):
            self.assertEqual(review(cpu,native,s),'native-current-use-v1')
            self.assertEqual(native['states'][0],dict(known=False,conflict=False,value=0))
        cpu,native,s=c['records'][0],c['native_compute']['records'][0],c['submission']['samples'][0]
        for field in ('frame','epoch','packet','context'):
            wrong=copy.deepcopy(cpu);wrong[field]+=1
            with self.subTest(cpu=field),self.assertRaises(ValueError):review(wrong,native,s)
        for role in (0,4,5):
            for field in ('owner','backing','resource','engine_width','engine_height','gpu_format'):
                wrong=copy.deepcopy(cpu);wrong['inputs'][role][field]+=1
                with self.subTest(role=role,field=field),self.assertRaises(ValueError):review(wrong,native,s)
        for field in ('command','reset'):
            wrong=dict(s);wrong[field]+=1
            with self.subTest(submission=field),self.assertRaises(ValueError):review(cpu,native,wrong)
        for field in ('packet','command','publication'):
            wrong=copy.deepcopy(native);wrong[field]+=1
            with self.subTest(native=field),self.assertRaises(ValueError):review(cpu,wrong,s)
        for field in ('accepted','packet_revalidated','attempted'):
            wrong=copy.deepcopy(native);wrong['color_admission'][field]=False
            with self.subTest(receipt=field),self.assertRaises(ValueError):review(cpu,wrong,s)
        for field in ('type','flags','subresource','before','after','resource'):
            wrong=copy.deepcopy(native);wrong['barriers'][0][field]+=1
            with self.subTest(barrier=field),self.assertRaises(ValueError):review(cpu,wrong,s)
        wrong=copy.deepcopy(native);wrong['color_admission']['state_admission']['results'][0]['stable']=False
        with self.assertRaises(ValueError):review(cpu,wrong,s)
        wrong=copy.deepcopy(native);wrong['color_admission']['state_admission']['accepted']+=1
        with self.assertRaises(ValueError):review(cpu,wrong,s)
        wrong=copy.deepcopy(native);wrong['states'][0]=dict(known=True,conflict=False,value=64)
        with self.assertRaises(ValueError):review(cpu,wrong,s)
        wrong=copy.deepcopy(native);del wrong['color_admission']
        with self.assertRaises(KeyError):review(cpu,wrong,s)

    def test_scaled_warp_capture(self):
        self.review_scaled_warp_capture()

    def test_colorgrade_version_shared_reviewers(self):
        # The current outer header must reach every shared input reviewer.
        # Header-only tests missed the 1.23.10 resource-count/geometry branches.
        self.review_scaled_warp_capture(current_colorgrade=True)

    def test_draw_execution_version_shared_reviewers(self):
        self.review_scaled_warp_capture(current_colorgrade=True,execution=True)

    def review_scaled_warp_capture(self,*,current_colorgrade=False,execution=False):
        if not hasattr(sys,'argv_fixture'):self.skipTest('retained WARP fixture required')
        import test_temporal_inputs as fixture
        fixture.ReviewTests.setUpClass()
        rows=copy.deepcopy(fixture.ReviewTests.rows);h=rows[0];c=rows[4]
        drawing=c['depth_routes']['consumers'][0].get('operation')=='draw'
        h.update(version='1.23.3' if drawing else '1.23.0',hooks=21 if drawing else 20,depth_draw_capture=drawing,depth_draw_hooks=1 if drawing else 0,scaled_temporal=True,scale=.5,color_state_admission="native-current-use-v1",color_state_proof_limit=3,
                 pre_unhook_cutoff=True,journal_drain=True,packet_generations=True,sdk_evaluation_limit=0)
        if 'projection_stimulus' in c:
            scoped=c['projection_stimulus'].get('frame_scope_required') is True
            depth_gate=c['depth_routes'].get('projection_gate',{}).get('required') is True
            h.update(version='1.23.6' if depth_gate else '1.23.5' if scoped else '1.23.4',depth_projection_gate=depth_gate,projection_activation_scope=scoped,hooks=22,jitter_writes=1,scaled_projection=True,projection_samples=3,
                     projection_byte_limit=4096,jitter_restorations=1,jitter_min_abs=.3125,jitter_selection_limit=20)
            if 'final_handoff' in c:
                h.update(version='1.23.7',hooks=23,final_handoff_observer=True,final_handoff_samples=3,final_handoff_mutations=0)
            if current_colorgrade:
                if 'final_handoff' not in c or not depth_gate:
                    self.skipTest('current scaled handoff fixture required')
                h.update(version='1.23.10',hooks=25,downstream_observer=True,
                    downstream_samples=3,downstream_packet_limit=8,downstream_filter_limit=16,
                    postfilter_observer=True,postfilter_samples=3,postfilter_command_limit=32,
                    postfilter_shader_limit=262144,postfilter_mutations=0,colorgrade_observer=True,
                    colorgrade_scan_limit=65535,colorgrade_shader_limit=262144)
                if execution:
                    h.update(version='1.23.12',hooks=26,draw_execution_observer=True,
                        draw_execution_samples=9,draw_execution_shader_limit=262144)
            self.assertTrue(scaled.base.review_jitter(h,c)['projection_taa_request_association_verified'])
        elif current_colorgrade:
            self.skipTest('current projection fixture required')
        self.assertEqual(scaled.parse(json.dumps(h).encode())[0],h)
        self.assertTrue(scaled.base.review(rows,cpu_only=True)['accepted'])
        if current_colorgrade:
            self.assertEqual(c['pins_acquired'],24)
            for field in ('pins_acquired','pins_released'):
                for count in (21,23,25):
                    wrong=copy.deepcopy(rows);wrong[4][field]=count
                    with self.subTest(field=field,count=count),self.assertRaisesRegex(ValueError,'resource references'):
                        scaled.base.review(wrong,cpu_only=True)
        folder=fixture.ReviewTests.shader.parent
        # Real retained WARP resources/copies/conversions; scale hook geometry
        # and game identity remain separate native fixtures, not live evidence.
        for function,args,kwargs in (
            (scaled.base.review_shader,(c,fixture.ReviewTests.shader),{}),
            (scaled.base.review_consumer,(h,c),{}),
            (scaled.base.review_submission,(c,),dict(sample_count=3)),
            (scaled.base.review_motion,(h,c,folder),{}),
            (scaled.base.review_color,(h,c,folder),{}),
            (scaled.base.review_scene_depth,(c,),dict(sample_count=3,paired_jitter=h['version'] in ('1.23.4','1.23.5','1.23.6','1.23.7','1.23.10','1.23.11','1.23.12'))),
            (scaled.base.review_depth_routes,(h,c),dict(selected_scope=True)),
            (scaled.base.review_depth_pixels,(h,c,folder),{}),
            (scaled.base.review_owned,(h,c,folder),{}),
        ):
            with self.subTest(stage=function.__name__):function(*args,**kwargs)
        self.assertEqual(scaled.scales(c,dict(display=[17,13],render=[8,6]))['active_region'],[8,6])
        if 'final_handoff' in c:
            from temporal_final_handoff import review
            self.assertTrue(review(h,c)['native_final_copy_construction_verified'])
        if h['version'] in ('1.23.6','1.23.7','1.23.10','1.23.11','1.23.12'):
            self.assertEqual(c['depth_routes']['projection_gate']['waits'],102)
            wrong=dict(h,depth_projection_gate=False)
            with self.assertRaises(ValueError):scaled.parse(json.dumps(wrong).encode())
            for key,value in [('required',False),('opened',False),('waits',-1),('waits',True)]+[(k,0) for k in ('context','view','container','frame','epoch','bank')]:
                wrong=copy.deepcopy(c);wrong['depth_routes']['projection_gate'][key]=value
                with self.subTest(gate=key),self.assertRaises(ValueError):scaled.base.review_depth_routes(h,wrong,selected_scope=True)
            wrong=copy.deepcopy(c);wrong['depth_routes']['projection_gate']['async']^=1
            with self.assertRaises(ValueError):scaled.base.review_depth_routes(h,wrong,selected_scope=True)
        for key,value in [('packet_generations',False),('gpu_commands',0),('hooks',10),('scale',1)]:
            wrong=dict(h);wrong[key]=value
            with self.subTest(header=key),self.assertRaises(ValueError):scaled.parse(json.dumps(wrong).encode())
        if drawing:
            from temporal_scaled_geometry import depth_operation,active_rectangle
            # Exercise the complete state reviewer, retaining all real list/
            # generation evidence while changing only source transitions.
            def with_source_state(value):
                wrong=copy.deepcopy(c)
                for consumer in wrong['depth_routes']['consumers']:
                    resource=wrong['depth_routes']['requests'][consumer['receipt']]['source']['resource']
                    for call in consumer['barrier_calls']:
                        for barrier in consumer['barriers'][call['first']:call['first']+call['count']]:
                            if barrier['type']==0 and barrier['resource']==resource:
                                if call['exit']<consumer['copy']:barrier['after']=value
                                else:barrier['before']=value
                return wrong
            for state in (0x80,0xc0):
                allowed=with_source_state(state)
                scaled.base.review_depth_routes(h,allowed,selected_scope=True)
                scaled.base.review_depth_pixels(h,allowed,folder)
                historical=dict(h,version='1.23.1',hooks=21,jitter_writes=0)
                self.assertEqual(scaled.parse(json.dumps(historical).encode())[0],historical)
                if state==0x80:
                    with self.assertRaisesRegex(ValueError,'copy/state association'):
                        scaled.base.review_depth_routes(historical,allowed,selected_scope=True)
                    with self.assertRaisesRegex(ValueError,'copy-point states'):
                        scaled.base.review_depth_pixels(historical,allowed,folder)
                else:
                    scaled.base.review_depth_routes(historical,allowed,selected_scope=True)
                    scaled.base.review_depth_pixels(historical,allowed,folder)
            for state in (0,0x40,0x10,0x4,0x90,0x88,0x180,0xc4,0x8c0,0x80000080):
                with self.subTest(source_state=state),self.assertRaisesRegex(ValueError,'copy/state association'):
                    scaled.base.review_depth_routes(h,with_source_state(state),selected_scope=True)
                with self.subTest(pixel_source_state=state),self.assertRaisesRegex(ValueError,'copy-point states'):
                    scaled.base.review_depth_pixels(h,with_source_state(state),folder)
            for kind in ('missing','split','partial','alias','conflict'):
                wrong=copy.deepcopy(c);consumer=wrong['depth_routes']['consumers'][0]
                b=consumer['barriers'][0]
                if kind=='missing':b['type']=2
                if kind=='split':b['flags']=1
                if kind=='partial':b['subresource']=0
                if kind=='alias':b.update(type=1,resource=0,other=0)
                if kind=='conflict':
                    # Replace the destination barrier with a contradictory
                    # second source transition; neither state is trustworthy.
                    consumer['barriers'][1]=dict(b,before=0,after=0x80)
                with self.subTest(barrier=kind),self.assertRaises(ValueError):
                    scaled.base.review_depth_routes(h,wrong,selected_scope=True)
            r=c['depth_routes']['consumers'][0];q=c['depth_routes']['requests'][r['receipt']]
            for key,value in [('stable',False),('recorded_packet',0),('pipeline',0),('rectangle',[0,0,17,13]),('arguments',[3,2,0,0])]:
                wrong=copy.deepcopy(r);wrong['draw'][key]=value
                with self.subTest(draw=key),self.assertRaises(ValueError):depth_operation(h,wrong,q)
            cpu=copy.deepcopy(c['records'][0]);cpu['scale_state']['frame']+=1
            with self.assertRaises(ValueError):active_rectangle(h,cpu,17,13)
            owned=scaled.base.review_owned(h,c,folder)
            self.assertEqual(owned['owned_compared_float_values'],8*6*7*3)
            # Unused backing pixels are not temporal inputs. A rehashed bad
            # active pixel must still fail independently of file integrity.
            with tempfile.TemporaryDirectory() as temp:
                dst=Path(temp)
                for file in folder.glob('inputs-123-456.*'):shutil.copyfile(file,dst/file.name)
                wrong=copy.deepcopy(c);entry=wrong['owned_gpu_inputs']['sets'][0]['files'][1];path=dst/entry['file']
                import struct
                raw=bytearray(path.read_bytes());raw[16*4:17*4]=struct.pack('<f',10)
                path.write_bytes(raw);entry['sha256']=hashlib.sha256(raw).hexdigest()
                scaled.base.review_owned(h,wrong,dst)
                raw[0:4]=struct.pack('<f',10);path.write_bytes(raw);entry['sha256']=hashlib.sha256(raw).hexdigest()
                with self.assertRaisesRegex(ValueError,'active-image comparison'):scaled.base.review_owned(h,wrong,dst)

    def test_frame_and_view_alignment(self):
        s=dict(display=[2560,1440],render=[1280,720])
        c=dict(width=2560,height=1440,records=[dict(frame=i+1,view=100,
            scale_state=dict(stable=True,frame=i+1,view=100,active=.5,content=1,display=s['display'],render=s['render']),
            rectangle=[0,0,2560,1440],groups=[160,90,1]) for i in range(3)])
        self.assertEqual(scaled.scales(c,s)['active_region'],[1280,720])
        for key,value in [('frame',9),('view',101),('active',1),('content',.5),('render',[2560,1440]),('stable',False)]:
            wrong=copy.deepcopy(c);wrong['records'][1]['scale_state'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):scaled.scales(wrong,s)

    def test_wrong_header_never_arms(self):
        for h in [dict(version='1.22.1'),dict(version='1.23.0',scaled_temporal=True,scale=.5,color_state_admission="native-current-use-v1",color_state_proof_limit=3,hooks=10)]:
            with self.assertRaises(ValueError):scaled.parse(json.dumps(h).encode())

    def test_process_generation_and_phase(self):
        h=dict(event='start',pid=10,creation=100,dll_sha256='a'*64)
        rows=[h,dict(event='reservation',ready=True,slot=10),dict(event='ready',hooks_installed=False)]
        validate(rows,10,100,'a'*64)
        for pid,creation in [(11,100),(10,101)]:
            with self.assertRaises(ValueError):validate(rows,pid,creation,'a'*64)
        with self.assertRaises(ValueError):validate(rows+[dict(event='capture')],10,100,'a'*64)

if __name__=='__main__':
    if len(sys.argv)>1 and not sys.argv[1].startswith('-'):sys.argv_fixture=sys.argv.pop(1)
    unittest.main()

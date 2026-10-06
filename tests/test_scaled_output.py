import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_scaled_output import header_contract,route_contract
from temporal_depth_routes import review_marker_coverage

def header():
    return dict(version='1.24.0',full_output_route=True,output_receipt=2,output_draw_limit=3,output_copy_limit=1,native_taa_replacement_limit=1,
        binding_hooks=23,inline_receipt=2,presentation_writes=1,sdk_evaluation_limit=1,sdk_create_limit=1,binding_observer=True,inline_same_list=True)

def capture():
    r=dict(schema=1,receipt=2,fault=0,pending=0,failure='',claimed=True,resolved=True,retired=True,hooks_disabled=True,
           prepared_draws=9,replaced_draws=3,copy_count=1,command=100,generation=1,frame=20,copy_packet=200,copy_event=0,draw_events=[1,2,3])
    return dict(inline_dlaa=dict(full_output_route=r),submission=dict(samples=[{}, {},dict(command=100,reset=1,frame=20,dispatch_exit=3,close=14)],
         events=[dict(kind=4,object=100,valid=True,returned=True,enter=4+i*2,exit=5+i*2) for i in range(4)]),
         final_handoff=dict(records=[{}, {},dict(packet=dict(object=200))],downstream=dict(postfilter=dict(draw_execution=dict(records=[dict(api_calls=1,replaced=i>=6,command=100,frame=20) for i in range(9)])))),
         native_compute=dict(records=[dict(dispatch_replaced=i==2,native_dispatch_forwards=int(i!=2)) for i in range(3)]))

class OutputReview(unittest.TestCase):
    def test_accepts_completed_route(self):route_contract(capture())
    def test_depth_and_output_marker_ownership(self):
        c=capture();c['submission']['events'].append(dict(kind=4))
        review_marker_coverage(header(),c,{4})
        # Earlier captures still require every marker to be a depth marker.
        review_marker_coverage({},c,set(range(5)))
        with self.assertRaisesRegex(ValueError,'depth marker coverage'):review_marker_coverage({},c,{4})
    def test_marker_ownership_rejects_orphans_and_overlap(self):
        c=capture();c['submission']['events'].append(dict(kind=4))
        for depth in (set(),{0,4},{4,5}):
            with self.subTest(depth=depth),self.assertRaises(ValueError):review_marker_coverage(header(),c,depth)
        c['submission']['events'].append(dict(kind=4))
        with self.assertRaisesRegex(ValueError,'depth marker coverage'):review_marker_coverage(header(),c,{4})
    def test_output_markers_require_valid_contract(self):
        c=capture()
        for field,value in [('fault',1),('retired',False),('command',101),('draw_events',[1,1,3])]:
            wrong=copy.deepcopy(c);wrong['inline_dlaa']['full_output_route'][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review_marker_coverage(header(),wrong,set())
        with self.assertRaisesRegex(ValueError,'full output version'):review_marker_coverage(dict(header(),version='1.23.12'),c,set())
    def test_requires_retirement_and_bounded_counts(self):
        for field,value in [('fault',1),('retired',False),('hooks_disabled',False),('prepared_draws',8),('copy_count',2),('replaced_draws',2),('generation',2),('command',101),('copy_packet',201)]:
            c=capture();c['inline_dlaa']['full_output_route'][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):route_contract(c)
    def test_rejects_different_or_unsubmitted_markers(self):
        for field,value in [('object',101),('kind',3),('returned',False),('valid',False),('enter',2),('exit',15)]:
            c=capture();c['submission']['events'][0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):route_contract(c)
        c=capture();c['inline_dlaa']['full_output_route']['draw_events']=[1,1,3]
        with self.assertRaises(ValueError):route_contract(c)
    def test_rejects_native_taa_double_execution(self):
        c=capture();c['native_compute']['records'][2]['native_dispatch_forwards']=1
        with self.assertRaises(ValueError):route_contract(c)
    def test_rejects_unreplaced_or_foreign_draw(self):
        for field,value in [('api_calls',2),('replaced',False),('command',101),('frame',21)]:
            c=capture();c['final_handoff']['downstream']['postfilter']['draw_execution']['records'][7][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):route_contract(c)
    def test_header_budget(self):
        h=header()
        header_contract(h)
        shared=dict(h,version='1.24.1',hooks=49,viewport_hook_owner='binding-observer',resolution_viewport_listener=True)
        header_contract(shared)
        isolated=dict(shared,version='1.24.2',binding_hook_control='owned-targets-only')
        header_contract(isolated)
        header_contract(dict(isolated,version='1.24.3'))
        header_contract(dict(isolated,version='1.24.4'))
        header_contract(dict(isolated,version='1.24.5'))
        for policy in (None,'global-queue'):
            wrong=dict(isolated,binding_hook_control=policy)
            with self.subTest(policy=policy),self.assertRaises(ValueError):header_contract(wrong)
        for key,value in (('hooks',50),('viewport_hook_owner','resolution'),('resolution_viewport_listener',False)):
            wrong=copy.deepcopy(shared);wrong[key]=value
            with self.subTest(shared=key),self.assertRaises(ValueError):header_contract(wrong)
        for key in ('output_receipt','output_draw_limit','output_copy_limit','native_taa_replacement_limit','binding_hooks','inline_receipt','presentation_writes','sdk_evaluation_limit','sdk_create_limit'):
            wrong=copy.deepcopy(h);wrong[key]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):header_contract(wrong)
if __name__=='__main__':unittest.main()

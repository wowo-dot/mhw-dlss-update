import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from aa_queue_layouts import review_return, review_layouts

BASE=0x140000000

def receipt(haze):
    nodes=[dict(descriptor=100,unit=1000,vtable=BASE+0x3538400,phase=15,order=0,flags=[0,0,0],input=10,output=20),
           dict(descriptor=200,unit=2000,vtable=BASE+0x3537a70,phase=15,order=100,flags=[0,0,0],input=20,output=30),
           dict(descriptor=300,unit=3000,vtable=BASE+0x3536c98,phase=15,order=1024,flags=[0,1,0],input=0,output=30),
           dict(descriptor=400,unit=0,vtable=0,phase=15,order=134217713,flags=[1,0,0],input=30 if haze else 20,output=40)]
    if haze:nodes.append(dict(descriptor=500,unit=5000,vtable=BASE+0x35372b0,phase=15,order=20000,flags=[0,0,0],input=0,output=0))
    before=dict(manager=1,context=2,view=3,renderer=4,frame=5,epoch=5,bank=0,rectangle=[0,0,2560,1440],
                enabled=1,collecting=1,taa=0,fxaa=0,scratch=[20,30],attachments=[10,40],count=len(nodes),packets=33,nodes=nodes)
    assigned=copy.deepcopy(before);assigned['nodes'][3]['input']=20 if haze else 30
    if haze:assigned['nodes'][4].update(input=30,output=20)
    inserted=dict(descriptor=900,unit=901,vtable=902,phase=15,order=2,flags=[0,1,0],input=0,output=20)
    callback=dict(copy.deepcopy(assigned),collecting=0,packets=40,count=len(nodes)+1,
                  nodes=sorted(copy.deepcopy(assigned['nodes'])+[inserted],key=lambda n:n['order']))
    after=dict(copy.deepcopy(callback),packets=53)
    restored=dict(copy.deepcopy(assigned),collecting=0,packets=53)
    return dict(reason=0,callbacks=1,inserted=True,removed=True,returned=True,complete=True,callback_valid=True,
                neutral=True,published=True,descriptor=900,unit=901,vtable=902,color_owner=20,
                before=before,assigned=assigned,callback=callback,after=after,restored=restored)

def ledger():
    return dict(frames=20,scheduler_layouts=dict(policy='base-or-haze-v1',base_admissions=10,haze_admissions=10,
        base_returns=10,haze_returns=10,base_evaluations=10,haze_evaluations=10,haze_entries=1,haze_exits=1,
        first_haze=receipt(True),first_base_after_haze=receipt(False),first_failed_return=None))

class LayoutReview(unittest.TestCase):
    def test_appearance_and_removal(self):
        self.assertIsNone(review_return(receipt(True),BASE,5))
        self.assertIsNone(review_return(receipt(False),BASE,4))
        self.assertTrue(review_layouts(ledger(),BASE)['haze_layout_verified'])
    def test_every_assigned_endpoint(self):
        for haze in (False,True):
            for stage in ('assigned','callback','after','restored'):
                for i in range(len(receipt(haze)[stage]['nodes'])):
                    for field in ('input','output'):
                        s=receipt(haze);s[stage]['nodes'][i][field]+=1
                        with self.subTest(haze=haze,stage=stage,index=i,field=field),self.assertRaises(ValueError):review_return(s,BASE,5 if haze else 4)
    def test_collection_values_are_not_final_endpoints(self):
        s=receipt(True)
        for n in s['before']['nodes']:n.update(input=0xABC,output=0xDEF)
        self.assertIsNone(review_return(s,BASE,5))
    def test_native_identity_and_restored_order(self):
        for field,value in [('phase',14),('order',2),('vtable',123),('flags',[0,1,0]),('unit',0)]:
            s=receipt(True);s['before']['nodes'][4][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review_return(s,BASE,5)
        s=receipt(True);s['restored']['nodes'].reverse()
        with self.assertRaises(ValueError):review_return(s,BASE,5)
    def test_receipt_and_counter_failures(self):
        for key,value in [('haze_returns',9),('base_evaluations',9),('haze_entries',12),('haze_exits',9),
                          ('first_failed_return',{}),('first_haze',None),('first_base_after_haze',None)]:
            s=ledger();s['scheduler_layouts'][key]=value
            with self.subTest(key=key),self.assertRaises((ValueError,TypeError)):review_layouts(s,BASE)
        for key in ('removed','returned','published','complete'):
            s=receipt(True);s[key]=False
            with self.subTest(key=key),self.assertRaises(ValueError):review_return(s,BASE,5)
    def test_base_only_does_not_verify_haze(self):
        s=ledger()['scheduler_layouts'];s.update(haze_admissions=0,haze_returns=0,haze_evaluations=0,haze_entries=0,haze_exits=0,
                                               first_haze=None,first_base_after_haze=None)
        self.assertFalse(review_layouts(dict(frames=10,scheduler_layouts=s),BASE)['haze_layout_verified'])

if __name__=='__main__':unittest.main()

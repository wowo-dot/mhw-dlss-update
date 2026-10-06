import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_draw_execution import review

class DrawExecutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path=Path(sys.argv_fixture);capture=json.loads(cls.path.read_text());cls.folder=cls.path.parent
        # Synthetic outer construction identities; native callback output and
        # compiled fixture shaders are real. No game/GPU draw claim is made.
        records=[];downstream=[]
        for f in range(3):
            a,b,c=capture['records'][3*f:3*f+3]
            geometry=lambda r:dict(packet=r['packet'],count=r['arguments'][0],first=r['arguments'][1])
            owner=lambda r:dict(resource=r['resource'],width=r['width'],height=r['height'])
            shader=lambda r:dict(sha256=r['shaders'][1]['sha256'],shader_size=r['shaders'][1]['size'])
            records.append(dict(frame=a['frame'],epoch=a['epoch'],view=a['view'],filter_geometry=geometry(a),
                final_geometry=geometry(c),output=owner(a),colorgrade=dict(geometry=geometry(b),output=owner(b),**shader(b)),**shader(a)))
            downstream.append(dict(destination=owner(c)))
        cls.downstream=dict(records=downstream,postfilter=dict(records=records,draw_execution=capture))
    def test_accept_native_callback_fixture(self):
        result=review({},self.downstream,self.folder)
        self.assertTrue(result['native_output_draw_execution_verified'])
        self.assertFalse(result['output_draw_gpu_submission_verified'])
        self.assertFalse(result['full_output_handoff_verified'])
    def test_reject_bad_draw_association(self):
        for field,value in [('index',9),('frame',99),('epoch',99),('view',1),('stage',2),('packet',1),('recorded_packet',1),
            ('arguments',[7,0]),('valid',False),('returned',False),('active_attachment',1),('color_count',2),('resource',1),
            ('rectangle',[0,0,16,16]),('viewport',[0,0,16,16]),('pipeline',0),('root',0),('width',99),('samples',2),('flags',0)]:
            wrong=copy.deepcopy(self.downstream);wrong['postfilter']['draw_execution']['records'][0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review({},wrong,self.folder)
    def test_reject_cleanup_or_coverage(self):
        for field,value in [('fault',1),('pending',1),('completed',8),('hooks_disabled',False),('drained',False),('gpu_submission_verified',True)]:
            wrong=copy.deepcopy(self.downstream);wrong['postfilter']['draw_execution'][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review({},wrong,self.folder)
    def test_reject_shader_damage(self):
        for field,value in [('sha256','0'*64),('size',52),('file','../outside'),('saved',False)]:
            wrong=copy.deepcopy(self.downstream);wrong['postfilter']['draw_execution']['records'][0]['shaders'][1][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review({},wrong,self.folder)
        wrong=copy.deepcopy(self.downstream);wrong['postfilter']['records'][0]['sha256']='0'*64
        with self.assertRaises(ValueError):review({},wrong,self.folder)
        wrong=copy.deepcopy(self.downstream);wrong['postfilter']['draw_execution']['records'][0]['input_elements'][0]['semantic']='x'*64
        with self.assertRaises(ValueError):review({},wrong,self.folder)

if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()

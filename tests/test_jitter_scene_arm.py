import json
import unittest
import test_jitter_scene_review as fixture
from arm_jitter_scene import validate_request
from review_submission import ReviewError

class JitterSceneArmTests(unittest.TestCase):
    def setUp(self):
        f=fixture.JitterSceneReviewTests();f.setUp()
        f.state.update(count=16,scene=0,armed=0,stop=0)
        self.state=f.state
        self.lines=['0\ttrace.start version=0.15.0 mode=test','1\tbridge.jitter-scene-event ready=true schema=1']
        self.lines+=['2\tbridge.jitter-sample '+json.dumps(r) for r in f.rows[:16]]
        self.preflight=dict(read_only=True,writes=0,live_code_verified=True,creation=123,channels=[dict(producer=1500)]*2)
    def text(self):return '\n'.join(self.lines+['3\tbridge.jitter-sequence-state '+json.dumps(self.state)])
    def test_valid_request(self):validate_request(self.preflight,123,self.text(),5000)
    def test_process_reuse_rejected(self):
        with self.assertRaisesRegex(ReviewError,'generation'):validate_request(self.preflight,124,self.text(),5000)
    def test_stale_window_rejected(self):
        with self.assertRaisesRegex(ReviewError,'five seconds'):validate_request(self.preflight,123,self.text(),116000)
    def test_duplicate_request_rejected(self):
        self.lines.append('bridge.jitter-scene-trigger accepted=true purpose=cpu-samples')
        with self.assertRaisesRegex(ReviewError,'already accepted'):validate_request(self.preflight,123,self.text(),5000)
    def test_missing_event_rejected(self):
        self.lines.pop(1)
        with self.assertRaisesRegex(ReviewError,'unavailable'):validate_request(self.preflight,123,self.text(),5000)
    def test_no_scene_after_trace_close(self):
        self.lines.append('trace.complete')
        with self.assertRaisesRegex(ReviewError,'active'):validate_request(self.preflight,123,self.text(),5000)

if __name__=='__main__':unittest.main()

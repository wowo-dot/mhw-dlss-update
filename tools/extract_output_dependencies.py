"""Extract private shaders and CPU resource edges from a verified dependency status."""
from pathlib import Path
import argparse,hashlib,json
from continuous_dependencies import review

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('status',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    if a.status.stat().st_size>8*1048576:raise ValueError('status byte bound')
    status=json.loads(a.status.read_text(encoding='utf-8-sig'));trace=status.get('scaled_route',status).get('dependencies',status)
    t=review(trace)
    if a.output.exists():raise ValueError('preserve prior output')
    a.output.mkdir(parents=True);edges=[];written=set()
    for si,s in enumerate(t['samples']):
        writers={}
        for di,d in enumerate(s['draws']):
            for stage,r in enumerate(d['ranges']):
                by_index={x['index']:x['fields'] for x in d['textures']}
                for word in r['words']:
                    if word&15!=2:continue
                    resource=by_index[(word>>4)&255][3]
                    if resource in writers:edges.append(dict(sample=si,producer=writers[resource],consumer=di,stage='ps' if stage else 'vs',register=word>>24,resource=resource,cpu_identity_edge_only=True))
            writers[d['target'][5]]=di
            for stage,h in enumerate(d['shaders']):
                data=bytes.fromhex(h.pop('hex'));digest=hashlib.sha256(data).hexdigest();name=digest+('.ps.dxbc' if stage else '.vs.dxbc')
                if name not in written:(a.output/name).write_bytes(data);written.add(name)
                h.update(sha256=digest,file=name)
    result=dict(schema=1,source_sha256=hashlib.sha256(a.status.read_bytes()).hexdigest(),capture=t,edges=edges,gpu_verified=False)
    (a.output/'capture.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(dict(samples=len(t['samples']),shader_files=len(written),edges=edges,gpu_verified=False)))
if __name__=='__main__':main()

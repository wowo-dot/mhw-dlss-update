"""Summarize saved complete R32F allocations, preserving outside-view pixels.

Distribution is evidence, not a declaration that every pixel is current-frame
depth. Native descriptor contents remain a separate question.
"""
import argparse,array,hashlib,json,math,sys
from pathlib import Path
from continuous_compute_depth_pixels import review
from arm_continuous_dlaa import require


def summarize(path,w,h):
    raw=path.read_bytes();require(len(raw)==w*h*4,'complete depth file size')
    values=array.array('f');values.frombytes(raw)
    if sys.byteorder!='little':values.byteswap()
    regions=[]
    for name,x0,y0,x1,y1 in [('active_view',0,0,w//2,h//2),('right',w//2,0,w,h//2),('bottom',0,h//2,w,h)]:
        finite=nonfinite=zero=one=outside=0;lo=math.inf;hi=-math.inf;digest=hashlib.sha256()
        for y in range(y0,y1):
            digest.update(raw[4*(y*w+x0):4*(y*w+x1)])
            for v in values[y*w+x0:y*w+x1]:
                if not math.isfinite(v):nonfinite+=1;continue
                finite+=1;zero+=v==0;one+=v==1;outside+=not 0<=v<=1;lo=min(lo,v);hi=max(hi,v)
        regions.append(dict(region=name,rectangle=[x0,y0,x1,y1],finite=finite,nonfinite=nonfinite,zero=zero,one=one,
                            outside_unit=outside,minimum=lo if finite else None,maximum=hi if finite else None,sha256=digest.hexdigest()))
    return dict(file=path.name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),regions=regions)


def main():
    p=argparse.ArgumentParser();p.add_argument('--status',type=Path,required=True);p.add_argument('--prefix',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    status=json.loads(a.status.read_text(encoding='utf-8-sig'));r=status['scaled_route'];trace=r['compute_depth_pixels']
    review(trace,r['compute_resources']);results=[]
    for i,c in enumerate(trace['captures']):
        require(c['saved'],'both depth samples saved')
        item=summarize(Path(str(a.prefix)+f'-gallery-depth-{i}.r32f'),c['width'],c['height'])
        item.update(sample=i,frame=c['frame'],packet=c['packet'],resource=c['resource']);results.append(item)
    result=dict(schema=1,accepted=True,scope='captured full native allocations before reduction',
                current_frame_coverage_inferred=False,descriptor_contents_verified=False,samples=results)
    with a.output.open('x') as f:json.dump(result,f,indent=2)
    print(json.dumps(result))


if __name__=='__main__':main()

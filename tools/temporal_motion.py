"""Validate bounded GBuffer4 readbacks; shader decoding is not motion validation."""
from temporal_limits import frame_count
from temporal_scaled_geometry import active_rectangle
import hashlib
import struct
from pathlib import Path

BYTE_LIMIT=16*1024*1024

def require(ok,reason):
    if not ok:raise ValueError(reason)

def decode_word(word):
    """Shader-derived (dx, dy) in NDC; low bit has unknown meaning."""
    require(type(word) is int and 0<=word<2**32,'packed motion word')
    return tuple(struct.unpack('<e',struct.pack('<H',v))[0] for v in (word>>16,word&0xfffe))

def history_uv_delta(word):
    dx,dy=decode_word(word)
    return -dx/2,dy/2

def review_readback(header,capture,folder,color=False):
    n=frame_count(header)
    tag='color' if color else 'motion'
    role,fmt,flags,state,prefix=(0,26,5,64,'color') if color else (5,42,1,192,'copy')
    readback=capture.get(tag+'_readback',{})
    require(readback.get('enabled') is True and readback.get('retention')=='process-lifetime-after-issue',tag+' retention policy')
    copies=readback.get('copies',[]);require(len(copies)==n,tag+' copy count')
    trace=capture['submission'];require(trace.get('copies_enabled') is True,tag+' submission mode')
    require(folder is not None,tag+' artifact directory');folder=Path(folder)
    # All orders share the submission clock, including gaps around native
    # Dispatch. Reject reuse against unrelated calls and private fence events.
    orders=set()
    for event in trace['events']:orders.update((event['enter'],event['exit']))
    for sample in trace['samples']:orders.update(sample[k] for k in ('signal','signal_exit','completed_order'))
    result=[]
    for i,(p,cpu,native,s) in enumerate(zip(copies,capture['records'],capture['native_compute']['records'],trace['samples'])):
        require(p.get('phase')==4 and all(p.get(k) is True for k in ('attempted','transitioned','copied','restored','retained','mapped')) and p.get('map_result')==0,tag+' copy closure')
        require(p['receipt']==i and p['frame']==cpu['frame'] and p['packet']==cpu['packet'],tag+' request association')
        require(p['command']==s['command']==native['command'] and p['allocator']==s['allocator'],tag+' command association')
        source=cpu['inputs'][role]
        require(p['resource']==s[prefix+'_resource']==source['resource'] and p['identity']==s[prefix+'_identity']==source['identity'],tag+' source association')
        w,h=p['width'],p['height'];require(type(w) is int and type(h) is int and 0<w<=8192 and 0<h<=8192 and w*h*4<=BYTE_LIMIT,tag+' extent')
        require((w,h)==(source['width'],source['height'])==(capture['width'],capture['height']),tag+' dimensions')
        require(cpu['rectangle']==active_rectangle(header,cpu,w,h),tag+' viewport')
        require([source[k] for k in ('dimension','format','mips','array_size','sample_count','sample_quality','layout','flags')]==[3,fmt,1,1,1,0,0,flags],tag+' resource description')
        if color and header['version'] in ('1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
            from temporal_color_state import review as review_state
            review_state(cpu,native,s)
        else:require(native['states'][role]==dict(known=True,conflict=False,value=state),tag+' source state')
        require([p[k] for k in ('format','source_flags','subresource','state_before','copy_state','state_after')]==[fmt,flags,0,state,2048,state],tag+' copy state')
        require(p['reset']==s['reset'] and p['marker']==s['dispatch'] and p['begin']==s[prefix+'_begin'] and p['end']==s[prefix+'_end'] and s[prefix+'_ok'] is True,tag+' copy witness')
        require(s['dispatch']<p['begin']<p['end']<s['dispatch_forward']<s['dispatch_exit'],tag+' copy order')
        extra={p['begin'],p['end'],s['dispatch_forward']}
        require(len(extra)==3 and not extra&orders,tag+' order collision');orders.update(extra)
        pitch=(4*w+255)&~255;allocation=pitch*(h-1)+4*w
        require(p['footprint']==[0,fmt,w,h,1,pitch] and p['row_bytes']==4*w and p['rows']==h and p['allocation_bytes']==allocation<=BYTE_LIMIT,tag+' footprint')
        require(p['pixel_bytes']==4*w*h,tag+' pixel byte count')
        name=f"{header['motion_prefix']}.{tag}-{i}.u32"
        require(p['file']==name and Path(name).name==name,tag+' artifact name')
        path=folder/name;require(path.stat().st_size==p['pixel_bytes'],tag+' artifact size')
        sha=hashlib.sha256(path.read_bytes()).hexdigest();require(p['sha256']==sha,tag+' artifact hash')
        result.append(dict(frame=p['frame'],file=name,sha256=sha,width=w,height=h,format='R11G11B10_FLOAT' if color else 'R32_UINT',bytes=p['pixel_bytes']))
    return result

def review_motion(header,capture,folder):
    result=review_readback(header,capture,folder)
    return dict(motion_readback_verified=True,motion_pixels_saved=True,motion_readbacks=result,
                motion_contents_verified=False,readback_eligible=False,dlaa_ready=False)

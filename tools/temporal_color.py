"""Validate paired TAA color copies and decode DXGI R11G11B10_FLOAT words."""
from temporal_limits import frame_count
import math
from temporal_motion import require,review_readback

def decode_color(word):
    # Windows SDK DirectXPackedVector.h XMFLOAT3PK / XMLoadFloat3PK:
    # unsigned R low 11, G middle 11, B high 10; exponent bias 15.
    require(type(word) is int and 0<=word<2**32,'packed color word')
    result=[]
    for offset,bits in ((0,6),(11,6),(22,5)):
        v=word>>offset;mantissa=v&((1<<bits)-1);exponent=(v>>bits)&31
        if exponent==31:value=math.nan if mantissa else math.inf
        elif exponent==0:value=math.ldexp(mantissa,1-15-bits)
        else:value=math.ldexp(1+mantissa/(1<<bits),exponent-15)
        result.append(value)
    return tuple(result)

def review_color(header,capture,folder):
    n=frame_count(header)
    trace=capture['submission'];require(trace.get('colors_enabled') is True,'color submission mode')
    result=review_readback(header,capture,folder,color=True)
    motion=capture['motion_readback']['copies'];color=capture['color_readback']['copies']
    frames=[r['frame'] for r in capture['records']]
    require(all(b==a+1 for a,b in zip(frames,frames[1:])),'paired frame adjacency')
    buffers=[p.get('readback_buffer',0) for p in motion+color]
    require(all(type(b) is int and 0<b<2**64 for b in buffers) and len(set(buffers))==2*n,'paired readback allocations')
    require(not set(buffers)&{p['resource'] for p in motion+color},'paired readback/source alias')
    require(sum(p['allocation_bytes'] for p in motion+color)<=header['total_readback_limit'],'paired allocation budget')
    orders={o for e in trace['events'] for o in (e['enter'],e['exit'])}
    orders.update(s[k] for s in trace['samples'] for k in ('signal','signal_exit','completed_order','dispatch_forward','copy_begin','copy_end'))
    for m,c,s in zip(motion,color,trace['samples']):
        require(all(m[k]==c[k] for k in ('receipt','frame','packet','command','allocator','reset','marker','width','height')),'paired copy association')
        require(m['resource']!=c['resource'] and m['identity']!=c['identity'],'paired source alias')
        require(m['end']<c['begin']<c['end']<s['dispatch_forward'],'paired copy order')
        require(not {c['begin'],c['end']}&orders,'paired copy order collision');orders.update((c['begin'],c['end']))
    return dict(color_readback_verified=True,color_pixels_saved=True,color_readbacks=result,
                paired_color_motion_readback_verified=True,color_motion_alignment_verified=False,
                motion_contents_verified=False,dlaa_ready=False)

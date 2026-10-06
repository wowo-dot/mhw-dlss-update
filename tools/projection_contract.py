"""Independent matrix and native-root-layout checks; no GPU-execution claim."""
import math
import struct

KNOWN_VERTEX = '94c34ad9564bb58ab799ade08682054ac6dff9b6402e7d02b2a2b2f2e8109511'

def require(ok, why):
    if not ok: raise ValueError(why)

def floats(bits, size):
    require(len(bits)==size and all(type(b) is int and 0<=b<2**32 for b in bits), 'matrix bit layout')
    values=struct.unpack('<'+'f'*size,struct.pack('<'+'I'*size,*bits))
    require(all(math.isfinite(v) for v in values), 'nonfinite matrix')
    return values

def matrices(source):
    require(source.get('matrices_valid') is True, 'matrix snapshot missing')
    ids=source['matrix_ids']+[source['projection_id'],source['unjittered_id']]
    require(len(ids)==5 and 0<source['words']<=1024, 'matrix identifiers')
    offsets=[]
    for value in ids:
        require(type(value) is int and 0<=value<0xffffffff and (value>>8)&15==15, 'matrix parameter shape')
        start=(value>>12)&0x3ff
        require(start+16<=source['words'] and all(start+16<=old or old+16<=start for old in offsets), 'matrix overlap/extent')
        offsets.append(start)
    vp=floats(source['view_projection_bits'],16);view=floats(source['view_bits'],16)
    plain_vp=floats(source['unjittered_view_projection_bits'],16)
    proj=floats(source['projection_bits'],16);plain_proj=floats(source['unjittered_bits'],16)
    jitter=floats(source['jitter_bits'],2);width,height=floats(source['viewport_bits'],2)
    require(width>0 and height>0 and any(vp[3::4]) and any(plain_vp[3::4]), 'matrix viewport/homogeneous column')
    maximum=0.
    # VP=(V*(P*J))*D, whereas the retained projection is (P*J)*D.
    # Reassociation and depth subtraction can introduce float32 rounding.
    # Bound by the dot-product magnitude, including W for reversed-depth Z.
    for result,right in ((vp,proj),(plain_vp,plain_proj)):
        for row in range(4):
            for col in range(4):
                terms=[view[4*row+k]*right[4*k+col] for k in range(4)]
                expected=sum(terms);actual=result[4*row+col]
                scale=sum(abs(v) for v in terms)
                if col==2:scale+=sum(abs(view[4*row+k]*right[4*k+3]) for k in range(4))
                residual=abs(actual-expected);maximum=max(maximum,residual)
                require(residual<=16*2**-23*max(1e-12,scale,abs(actual)), 'view-projection product mismatch')
    for row in range(4):
        for col in range(4):
            delta=plain_vp[4*row+3]*(jitter[0]/width if col==0 else jitter[1]/height if col==1 else 0)
            actual=vp[4*row+col];plain=plain_vp[4*row+col]
            require(abs((actual-plain)-delta)<=16*2**-23*max(1e-12,abs(actual),abs(plain),abs(delta)), 'view-projection jitter mismatch')
    if jitter==(0.,0.):require(vp==plain_vp, 'zero-jitter view-projection mismatch')
    return dict(matrix_products_verified=True,view_projection_jitter_relation=True,
                matrix_max_absolute_product_residual=maximum,matrix_word_offsets=offsets,
                nonzero_jitter_observed=any(jitter))

def root_layout(draw, apis, roots, shaders, source):
    s=draw.get('root_selection',{})
    require(s.get('valid') is True and s.get('stable') is True, 'root selection missing/changed')
    require(s['recipe']>0 and s['recipe_device']==draw['device'], 'root recipe device')
    stages=draw['stage_entries'];require(len(stages)==5, 'root stages')
    flags=s['recipe_flags'];require(len(flags)==3 and all(v in (0,1) for v in flags), 'root recipe flags')
    require(s['descriptor_mode']==draw['binding_mode']==flags[2] and draw['binding_mode'] in (0,1), 'root binding mode')
    index=4 if stages[3] else 3 if stages[2] else 1 if s['vs_flags']&s['ps_flags']&0x10000 else 0 if s['ps_count'] else 2
    require(index==s['index'] and len(s['signatures'])==5 and draw['root_cache']==s['signatures'][index]!=0, 'selected root identity')
    # Only the three fully mapped VS/PS root-CBV layouts are qualified here.
    base={0:18,1:9,2:2}.get(index) if draw['binding_mode']==0 else None
    active=None;bound={};fresh_signature=False;expected_bind=False
    for a in sorted(apis,key=lambda a:a['order']):
        if a['kind'] in (11,18):
            active=a['args'][0] if a['kind']==11 else None;bound.clear()
        if a['kind']==9:bound[a['args'][0]]=a['args'][1]
        if a['kind'] in (14,15,16) and active==draw['root_cache']:
            fresh_signature=any(k in roots and v==draw['gpu'] for k,v in bound.items())
            expected_bind=base is not None and base in roots and bound.get(base)==draw['gpu']
    known=shaders[draw['blobs'][0]-1]['sha256']==KNOWN_VERTEX
    mapped=expected_bind and fresh_signature and known and ((source['matrix_ids'][0]>>12)&0x3ff)==0
    return dict(native_root_selection_verified=True,native_root_variant=index,vs_cbv_root_base=base,
        actual_root_signature_call_observed=fresh_signature,known_vertex_contract=known,
        native_metadata_vs_b0_binding_correlated=mapped,executing_pso_bytecode_verified=False)

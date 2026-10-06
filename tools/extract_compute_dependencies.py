"""Extract private shaders and ordered CPU resource dependencies of compute bundles."""
import argparse, hashlib, json, math, struct
from pathlib import Path
from continuous_compute_dependencies import review


def decode_constant(c):
    data=bytes.fromhex(c['hex']);identity=c['word']>>12&4095
    def floats(offset,count):
        values=list(struct.unpack_from('<'+'f'*count,data,offset))
        return [v if math.isfinite(v) else str(v) for v in values]
    if identity==12:
        return dict(name='CBScreen',screen_size=floats(16,2),screen_inverse=floats(24,2),
                    view_offset=list(struct.unpack_from('<2I',data,32)),view_size=list(struct.unpack_from('<2I',data,40)),
                    float_view_offset=floats(48,2),float_view_size=floats(56,2),view_inverse=floats(64,2),
                    content_scales=floats(72,8),checkerboard=struct.unpack_from('<I',data,104)[0])
    if identity==11:return dict(name='CBViewProjection',z_to_linear=floats(416,3))
    return dict(name='CBNewDOFFilter',coc_bias_scale_pcoc=floats(0,3),radius_aspect=floats(36,2),
                ignore_flags=list(struct.unpack_from('<2I',data,44)),coefficients=floats(52,2),
                vignette_offset_power=floats(60,2),vignette_enable=struct.unpack_from('<I',data,68)[0],
                vignette_ellipticity=floats(72,1),vignette_color=floats(80,3))


def extract(trace, destination):
    t=review(trace)
    if destination.exists():raise ValueError('preserve existing extraction')
    destination.mkdir(parents=True);written=set();edges=[];external=[];constants=[];constant_files=set()
    for si,s in enumerate(t['samples']):
        writers={}
        for ordinal,command in enumerate(s['commands']):
            opcode=command[2]&31
            if opcode==11:continue
            compute=opcode==6;d=s['computes' if compute else 'draws'][command[3]]
            textures={x['index']:x['fields'] for x in d['textures']}
            ranges=[dict(words=d['words'])] if compute else d['ranges']
            pending=set()
            for stage,r in enumerate(ranges):
                for word in r['words']:
                    kind,index=word&15,(word>>4)&255
                    if kind not in (2,3):continue
                    fields=textures[index];resource=fields[3]
                    if kind==3:pending.add(resource);continue
                    edge=dict(sample=si,consumer=ordinal,stage='cs' if compute else 'ps' if stage else 'vs',
                              register=word>>24,owner=fields[0],resource=resource,extent=fields[4],engine_format=fields[5],cpu_identity_only=True)
                    if resource in writers:edges.append(dict(edge,producer=writers[resource]))
                    else:external.append(edge)
            if not compute:pending.add(d['target'][5])
            for resource in pending:writers[resource]=ordinal
            for stage,h in enumerate([d['shader']] if compute else d['shaders']):
                data=bytes.fromhex(h.pop('hex'));digest=hashlib.sha256(data).hexdigest()
                name=digest+('.cs.dxbc' if compute else '.ps.dxbc' if stage else '.vs.dxbc')
                if name not in written:(destination/name).write_bytes(data);written.add(name)
                h.update(sha256=digest,file=name)
            for c in d.get('constants',[]):
                data=bytes.fromhex(c['hex']);digest=hashlib.sha256(data).hexdigest();name=digest+'.cb.bin'
                if name not in constant_files:(destination/name).write_bytes(data);constant_files.add(name)
                constants.append(dict(sample=si,command=ordinal,owner=d['owners'][c['word']>>4&255][2],
                                      register=c['word']>>24,size=c['size'],sha256=digest,file=name,**decode_constant(c)))
    result=dict(schema=1,capture=t,edges=edges,external_reads=external,shader_files=sorted(written),
                constants=constants,constant_files=sorted(constant_files),gpu_verified=False)
    (destination/'capture.json').write_text(json.dumps(result,indent=2))
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('status',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.status.stat().st_size>16*1048576:raise ValueError('status byte bound')
    status=json.loads(a.status.read_text(encoding='utf-8-sig'));trace=status.get('scaled_route',status).get('compute_dependencies',status)
    r=extract(trace,a.output)
    print(json.dumps(dict(samples=len(r['capture']['samples']),shader_files=len(r['shader_files']),edges=len(r['edges']),external_reads=len(r['external_reads']),gpu_verified=False)))


if __name__=='__main__':main()

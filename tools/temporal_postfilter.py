"""Post-TAA CPU construction evidence; does not establish GPU consumption."""
import hashlib
import math
from pathlib import Path
from temporal_shader import require


def review(header, downstream, folder):
    p=downstream['postfilter']
    grade=header.get('colorgrade_observer',False)
    require(p['schema']==(2 if grade else 1) and p['policy']==('postfilter-construction-v2' if grade else 'postfilter-construction-v1'),'postfilter policy')
    require(p['fault']==p['pending']==p['mutations']==0 and p['selected']==p['completed']==3,'postfilter counts')
    require(p['hooks_disabled'] is True and p['drained'] is True,'postfilter cleanup')
    require(p['target']==header['base']+0x24ca1f0,'postfilter target')
    require(len(p['records'])==len(downstream['records'])==3,'postfilter records')
    aliases=[]
    for i,(r,d) in enumerate(zip(p['records'],downstream['records'])):
        require(r['index']==i and all(r[k] is True for k in ('valid','entered','returned','saved')),'postfilter qualification')
        require(all(r[k]==d[k] for k in ('frame','epoch','thread','context','view')),'postfilter frame association')
        require(r['caller']==header['base']+0x24f228b,'postfilter caller')
        filters=[f for f in d['filters'] if f['type']==header['base']+0x3537a70 and f['stage']==15 and f['order']==100]
        require(len(filters)==1 and filters[0]['unit']==r['unit'],'postfilter unit')
        require(r['input']['owner']==filters[0]['input'] and r['output']['owner']==filters[0]['output'],'postfilter filter ownership')
        require(r['input']['owner']==d['taa_destination'],'postfilter TAA output')
        require(all(r['final_source'][k]==d['source'][k] for k in ('owner','resource','width','height','format')),'postfilter final source')
        for owner in (r['input'],r['output'],r['final_source']):
            require(owner['owner']>0 and owner['resource']>0 and 0<owner['width']<=16384 and 0<owner['height']<=16384,'postfilter engine extent')
        require(r['rectangle']==r['returned_rectangle']==d['rectangle'],'postfilter restored rectangle')
        require(len(r['properties_168'])==4 and all(isinstance(v,int) and 0<=v<=0xffffffff for v in r['properties_168']),'postfilter properties')
        require(0<=r['count_before']<r['count_after']<65535 and r['count_after']-r['count_before']==len(r['commands'])<=32,'postfilter packet budget')
        draws=[c for c in r['commands'] if c['opcode']==0 and c['key']==(15<<27|100)]
        require(len(draws)==1,'postfilter draw identity')
        require(all(c['packet']>0 and 0<=c['opcode']<=31 and 0<=c['key']<=0xffffffff for c in r['commands']),'postfilter commands')
        for name,count in (('final_geometry',3),('filter_geometry',4)):
            g=r[name]
            require(all(g[k]>0 for k in ('packet','state','arena','vertices','graphics')) and g['count']==count and g['stride']==16,'postfilter geometry')
            require(0<=g['first']<=1048576 and 0<=g['offset']<=16777216 and g['vertices']==g['arena']+g['offset']+g['first']*16,'postfilter vertex address')
            require(len(g['values'])==16 and all(math.isfinite(v) for v in g['values']),'postfilter finite geometry')
        require(r['final_geometry']['packet']==d['packets'][0]['object'] and r['filter_geometry']['packet']==draws[0]['packet'],'postfilter packet links')
        g=r['final_geometry'];v=g['values'];rect=d['rectangle'];w,h=d['dimensions'];ox,oy=d['offset']
        expected=[-1,1,rect[0]/w,rect[1]/h,-1,-3,rect[0]/w,2*(rect[3]/h+oy),3,1,2*(rect[2]/w+ox),rect[1]/h]
        require(all(abs(a-b)<2e-6 for a,b in zip(v[:12],expected)),'postfilter final UV arithmetic')
        file=r['file'];require(isinstance(file,str) and Path(file).name==file and '/' not in file and '\\' not in file and ':' not in file,'postfilter artifact name')
        path=folder/file
        require(52<=r['shader_size']<=262144 and path.stat().st_size==r['shader_size'],'postfilter shader budget')
        raw=path.read_bytes()
        require(raw[:4]==b'DXBC' and hashlib.sha256(raw).hexdigest()==r['sha256'],'postfilter shader artifact')
        if grade:
            c=r['colorgrade'];g=c['geometry']
            require(all(c[k] is True for k in ('selected','stable','saved')),'colorgrade qualification')
            require(0<=c['command_index']<c['queue_count']<=r['count_before']<65535,'colorgrade command budget')
            units=[f for f in d['filters'] if f['type']==header['base']+0x3538400 and f['stage']==15 and f['order']==0]
            ends=[f for f in d['filters'] if f['stage']==15 and f['order']==0x7fffff1]
            require(len(units)==len(ends)==1 and units[0]['unit']==c['unit'] and ends[0]['entry']==c['endpoint']==c['unit']+0x238,'colorgrade endpoint ownership')
            require(ends[0]['unit']==0 and ends[0]['flags']==[1,0,0],'colorgrade deferred construction')
            require(c['input']==r['output'] and c['output']==r['final_source'] and ends[0]['input']==c['input']['owner'] and ends[0]['output']==c['output']['owner'],'colorgrade source/output chain')
            require(all(g[k]>0 for k in ('packet','state','arena','vertices','graphics')) and g['count']==4 and g['stride']==16,'colorgrade geometry')
            require(0<=g['first']<=1048576 and 0<=g['offset']<=16777216 and g['vertices']==g['arena']+g['offset']+g['first']*16,'colorgrade vertex address')
            require(len(g['values'])==16 and all(math.isfinite(v) for v in g['values']),'colorgrade finite geometry')
            file=c['file'];require(isinstance(file,str) and Path(file).name==file and '/' not in file and '\\' not in file and ':' not in file,'colorgrade artifact name')
            path=folder/file;require(52<=c['shader_size']<=262144 and path.stat().st_size==c['shader_size'],'colorgrade shader budget')
            raw=path.read_bytes();require(raw[:4]==b'DXBC' and hashlib.sha256(raw).hexdigest()==c['sha256'],'colorgrade shader artifact')
        aliases.append(r['output']['resource']==r['final_source']['resource'])
    return dict(postfilter_cpu_construction_verified=True,postfilter_output_aliases_final_source=aliases,
                colorgrade_cpu_construction_verified=grade,postfilter_gpu_consumption_verified=False,full_output_handoff_verified=False)

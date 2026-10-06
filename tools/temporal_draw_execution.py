"""Selected native draw execution, separate from GPU submission/completion."""
import hashlib,struct
from pathlib import Path
from temporal_shader import require

def review(header,downstream,folder):
    post=downstream['postfilter'];capture=post['draw_execution']
    require(capture['schema']==1 and capture['enabled'] is True,'draw execution enabled')
    require(capture['fault']==capture['pending']==capture['mutations']==0 and capture['completed']==9,'draw execution counts')
    require(capture['hooks_disabled'] is True and capture['drained'] is True,'draw execution cleanup')
    require(capture['gpu_submission_verified'] is False,'draw execution scope')
    records=capture['records'];require(len(records)==9,'nine output draws')
    summaries=[]
    for i,r in enumerate(records):
        p=post['records'][i//3];d=downstream['records'][i//3];stage=i%3
        g=[p['filter_geometry'],p['colorgrade']['geometry'],p['final_geometry']][stage]
        output=[p['output'],p['colorgrade']['output'],d['destination']][stage]
        require(r['index']==i and r['stage']==stage and r['valid'] is True and r['returned'] is True,'draw execution record')
        require(r['frame']==p['frame'] and r['epoch']==p['epoch'] and r['view']==p['view'],'draw execution frame')
        require(r['packet']==r['recorded_packet']==g['packet'] and r['arguments']==[g['count'],g['first']],'draw packet association')
        require(all(type(r[k]) is int and r[k]>0 for k in ('thread','executor','context','command','state','pipeline','root','attachment','rtv')),'draw state identity')
        require(r['active_attachment']==r['attachment'] and r['color_count']==1 and r['attachment_flags']&7==1,'draw attachment')
        require(r['resource']==output['resource'] and [r['width'],r['height']]==[output['width'],output['height']],'draw output identity')
        require(r['samples']==1 and r['flags']&1 and r['format']>0 and r['rtv_format']>0,'draw resource description')
        require(type(r['scissor_enabled']) is int and r['scissor_enabled'] in (0,1),'scissor enable')
        for key in ('rectangle','viewport','scissor'):
            require(len(r[key])==4 and all(type(v) is int for v in r[key]),'draw rectangle shape')
        # Stage 16 already draws to the full output; both stage 15 effects are
        # expected to operate on the low active rectangle before integration.
        extent=[r['width'],r['height']] if stage==2 else [r['width']//2,r['height']//2]
        require(r['rectangle']==r['viewport']==[0,0,*extent],'draw viewport geometry')
        if r['scissor_enabled']:require(r['scissor']==[0,0,*extent],'draw scissor geometry')
        for key,n in (('depth',13),('blend',82),('raster',11)):
            require(len(r[key])==n and all(type(v) is int and 0<=v<2**32 for v in r[key]),'draw pipeline metadata')
        require(0<=r['input_count']<=32 and len(r['input_elements'])==r['input_count'],'draw input layout')
        for e in r['input_elements']:
            require(isinstance(e['semantic'],str) and 0<len(e['semantic'])<64 and all(32<=ord(c)<127 for c in e['semantic']),'draw semantic')
            require(all(type(e[k]) is int and 0<=e[k]<2**32 for k in ('index','format','slot','offset','class','step')),'draw input element')
        require(len(r['shaders'])==2,'draw shader stages')
        for j,s in enumerate(r['shaders']):
            file=s['file'];require(isinstance(file,str) and Path(file).name==file and not any(c in file for c in '/\\:'),'draw shader name')
            path=folder/file;require(s['saved'] is True and 52<=s['size']<=262144 and path.stat().st_size==s['size'],'draw shader bounds')
            raw=path.read_bytes();require(raw[:4]==b'DXBC' and hashlib.sha256(raw).hexdigest()==s['sha256'],'draw shader identity')
            word=lambda at:struct.unpack_from('<I',raw,at)[0]
            require(word(24)==len(raw) and 0<word(28)<=64 and 32+4*word(28)<=len(raw),'draw shader container')
            programs=[]
            for n in range(word(28)):
                at=word(32+4*n);require(at<=len(raw)-8 and word(at+4)<=len(raw)-at-8,'draw shader chunk')
                if raw[at:at+4] in (b'SHEX',b'SHDR'):
                    require(word(at+4)>=8,'draw program bounds');programs.append(word(at+8))
            require(programs==[0x10050 if j==0 else 0x50],'draw shader model')
            if j==1 and stage!=2:
                original=p if stage==0 else p['colorgrade']
                require(s['sha256']==original['sha256'] and s['size']==original['shader_size'],'construction/execution shader equality')
        summaries.append(dict(frame=r['frame'],stage=stage,viewport=r['viewport'],format=r['format'],pipeline=r['pipeline'],root=r['root']))
    return dict(native_output_draw_execution_verified=True,output_draws=summaries,
        output_draw_gpu_submission_verified=False,full_output_handoff_verified=False)

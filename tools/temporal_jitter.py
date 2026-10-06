"""Validate scoped CPU projection stimulus and exact paired TAA frame association."""
from projection_contract import require,floats,matrices
from temporal_limits import frame_count

def review_jitter(header,capture):
    n=frame_count(header);j=capture['projection_stimulus']
    require([j.get(k) for k in ('fault','pending','completed','matched','injections','restorations')]==[0,0,n,n,n-2,n-2], 'projection stimulus completion')
    require(j.get('hooks_disabled') is True,'projection hook closure')
    records=j.get('records',[]);require(len(records)==n,'projection samples')
    if header['version'] in ('1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(header.get('projection_activation_scope') is True and j.get('frame_scope_required') is True and
                type(j.get('frame_scope_waits')) is int and j['frame_scope_waits']>=0 and
                all(r.get('frame_scope_admitted') is True for r in records),'owned projection frame scope')
    if header['version'] in ('1.6.1','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(type(j.get('selection_attempts')) is int and 1<=j['selection_attempts']<=20 and
                type(j.get('selection_skipped')) is int and j['selection_skipped']==j['selection_attempts']-1,'jitter selection count')
        require(type(j.get('selected_frame')) is int and j['selected_frame']==records[0]['frame']+1==records[1]['frame'] and
                type(j.get('selected_candidate')) is int and j['selected_candidate']==records[1]['candidate'],'jitter preview identity')
        pair=[j['selected_candidate']&0xffffffff,j['selected_candidate']>>32];x,y=floats(pair,2)
        require(.3125<=abs(x)<=.5 and .3125<=abs(y)<=.5,'jitter stimulus strength')
    require(len({r['candidate'] for r in records[1:-1]})==n-2,'distinct projection samples')
    previous_tick=capture['started_tick'];result=[]
    for i,(r,t) in enumerate(zip(records,capture['records'])):
        require(r.get('stage')==i and all(r.get(k) is True for k in ('valid','returned','acknowledged','restored','projection_valid','matrix_relation','provider_stable','matrices_valid')),'projection record validity')
        require(r.get('injected') is (0<i<n-1) and r.get('restored_bits')==0,'scoped jitter restoration')
        require(all(r[k]==t[k] for k in ('frame','epoch','bank','async','context','view')),'projection/TAA frame association')
        require(r['caller']==header['base']+0x228e834 and r['context']==capture['renderer']+0x710 and r['renderer']==capture['renderer'],'projection call site')
        require(r['camera_type'] in (header['base']+0x350d188,header['base']+0x33f82a8),'projection camera type')
        require(all(type(r[k]) is int and r[k]>0 for k in ('camera','table','constants','thread')),'projection identities')
        require(r['provider_use'] in (0,1) and type(r['provider_sizing']) is int and r['provider_sizing']&1==0 and r['wrapper']==0 and
                r['backend']==capture['backend'] and r['device']==capture['device'],'projection provider')
        require(previous_tick<=r['enter_tick']<=r['exit_tick']<=t['enter_tick']<capture['started_tick']+2000,'projection timing')
        previous_tick=t['exit_tick']
        if i:
            require(r['frame']==records[i-1]['frame']+1 and r['epoch']==records[i-1]['epoch']+1,'projection frame adjacency')
            require(all(r[k]==records[0][k] for k in ('camera','camera_type','table','view','async','viewport_bits','buffer_id','projection_id','unjittered_id','matrix_ids','words')),'projection sequence identity')
        require(type(r['candidate']) is int and 0<r['candidate']<2**64,'projection candidate bits')
        pair=[r['candidate']&0xffffffff,r['candidate']>>32];x,y=floats(pair,2)
        require(abs(x)<=.5 and abs(y)<=.5 and (x!=0 or y!=0),'projection candidate bounds')
        require(r['table20_bits']==pair and r['jitter_bits']==(pair if 0<i<n-1 else [0,0]),'projection selected jitter')
        viewport=floats(r['viewport_bits'],2)
        render=(capture['width'],capture['height'])
        if header['version'] in ('1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
            scale=t['scale_state'];render=tuple(scale['render']);display=tuple(scale['display'])
            require(scale['stable'] is True and scale['frame']==r['frame'] and scale['view']==r['view'] and
                    scale['active']==.5 and scale['content']==1 and display==(capture['width'],capture['height']) and
                    render==(capture['width']//2,capture['height']//2) and t['rectangle']==[0,0,*render],'projection render association')
            require(viewport in (render,display),'scaled projection viewport')
        else:require(viewport==render,'projection viewport')
        require(type(r['buffer_id']) is int and 0<=r['buffer_id']<0xffffffff,'projection buffer id')
        verified=matrices(r)
        p=floats(r['projection_bits'],16);plain=floats(r['unjittered_bits'],16);w,h=floats(r['viewport_bits'],2)
        jx,jy=floats(r['jitter_bits'],2)
        for row in range(4):
            for col in range(4):
                index=row*4+col;delta=plain[row*4+3]*(jx/w if col==0 else jy/h if col==1 else 0)
                require(abs((p[index]-plain[index])-delta)<=8*2**-23*max(1e-6,abs(p[index]),abs(plain[index]),abs(delta)), 'projection jitter relation')
        result.append(dict(frame=r['frame'],jitter=[jx,jy],projection_viewport=list(viewport),render_extent=list(render),
                           predicted_raster_shift=[jx*render[0]/(2*w),-jy*render[1]/(2*h)],**verified))
    return dict(scoped_projection_stimulus_verified=True,projection_taa_request_association_verified=True,
                projection_samples=result,color_jitter_verified=False,motion_jitter_inclusion_verified=False,dlaa_ready=False)

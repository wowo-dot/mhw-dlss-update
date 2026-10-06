"""Independent validation of bounded v0.3.0 CPU/command-recording evidence.

This never promotes a native view pointer to view-creation provenance or a
recorded barrier to a known GPU state. SDK field offsets are checked by the
separately archived SDK 10.0.26100.0 layout fixture.
"""
import struct
from projection_contract import floats


def require(ok, why):
    if not ok:
        raise ValueError('attachments: '+why)


def uint(value, bits=64):
    return type(value) is int and 0 <= value < 2**bits


def vector(value, n, bits=32):
    return isinstance(value, list) and len(value)==n and all(uint(x,bits) for x in value)


def snapshot(s, d):
    require(s['valid'] is True and s['command']==d['command'] and s['state']>0 and s['attachment']>0, 'snapshot identity')
    require(uint(s['flags'],32) and s['color_count']==s['flags']&7 and s['scissor_enabled'] in (0,1), 'attachment flags/count')
    require(vector(s['cached'],9,64) and len(s['views'])==9, 'attachment slots')
    for i,v in enumerate(s['views']):
        require(all(uint(v[k]) for k in ('owner','backend','resource','handle')) and uint(v['format'],32), 'view scalars')
        require(v['rotating']==v['rotation_index']==0, 'unsupported rotating view')
        if not v['owner']:
            require(all(value==0 for value in v.values()), 'absent view payload')
        else:
            require(all(v[k]>0 for k in ('backend','resource','handle','format')), 'partial view')
        if s['color_count']<=i<8:
            require(v['owner']==0, 'unused color slot')
    for key in ('viewport','scissor'):
        require(len(s[key])==4 and all(type(v) is int and -(2**31)<=v<2**31 for v in s[key]), key+' extent')
    p=s['pipeline']
    require(p['graphics']==d['graphics'] and p['shader_handle']>0 and p['render']>0, 'native graphics identity')
    require(0<=p['input_count']<=32 and (not p['input_count'] or p['input_layout']>0 and p['input_elements']>0), 'native input layout')
    require(all(p[k]>0 for k in ('depth_owner','depth_state','blend_owner','blend_state','raster_owner','raster_state')), 'native state owner')
    require(p['raster_variant'] in (0,1) and vector(p['depth'],13) and vector(p['blend'],82) and vector(p['raster'],11), 'native state bytes')
    fmt=s['views'][8]['format']
    require(p['dsv_format']=={53:55,44:45,39:40}.get(fmt,fmt), 'native depth format conversion')


def creation(p, events, d, s, shaders):
    require(type(p['seen']) is bool and len(events)==int(p['seen']), 'creation event count')
    if not p['seen']:
        require(not any(p[k] for k in ('iid_valid','bytecode_matches','returned','succeeded','stable','pso','root','descriptor_address','output_address')), 'invented creation')
        return False
    e=events[0]
    require(all(p[k] is True for k in ('iid_valid','bytecode_matches','returned','succeeded','stable')), 'creation incomplete')
    require(uint(p['result'],32) and p['result']<0x80000000 and p['pso']==d['pso_cache']!=0 and p['root']==d['root_cache'], 'creation result/selection')
    require(e['args'][0]==p['descriptor_address']>0 and e['args'][1]>0 and e['args'][2]==p['output_address']>0, 'creation arguments')
    raw=bytes.fromhex(p['descriptor_hex'])
    require(len(raw)==656, 'creation descriptor length')
    def u(off):return struct.unpack_from('<I',raw,off)[0]
    def q(off):return struct.unpack_from('<Q',raw,off)[0]
    require(q(0)==p['root'] and u(0x240)==p['color_count']==s['color_count'] and
            u(0x264)==p['dsv_format']==s['pipeline']['dsv_format'], 'creation attachment formats')
    require(p['formats']==list(struct.unpack_from('<8I',raw,0x244))==[v['format'] for v in s['views'][:8]], 'creation color formats')
    require(u(0x268)==p['samples']>0 and u(0x26c)==p['quality'], 'creation sampling')
    require(vector(p['sizes'],5) and vector(p['data'],5,64), 'creation stage bounds')
    for i,off in enumerate((8,0x18,0x48,0x38,0x28)):
        require(q(off)==p['data'][i] and q(off+8)==p['sizes'][i], 'creation stage descriptor')
        index=d['blobs'][i]
        require(p['sizes'][i]==(shaders[index-1]['size'] if index else 0) and bool(p['data'][i])==bool(index), 'creation captured bytecode')
    require(0<=p['input_count']==u(0x230)<=32 and p['input_count']==s['pipeline']['input_count'], 'creation input count')
    elements=bytes.fromhex(p['input_elements_hex'])
    require(len(elements)==32*p['input_count'] and len(p['semantics'])==p['input_count'], 'creation input payload')
    require(not p['input_count'] or q(0x228)==s['pipeline']['input_elements'], 'creation input pointer')
    for i,name in enumerate(p['semantics']):
        require(isinstance(name,str) and len(name)<64 and all(32<=ord(ch)<127 for ch in name) and
                struct.unpack_from('<Q',elements,i*32)[0]>0, 'creation semantic')
    native=s['pipeline']
    require(list(struct.unpack_from('<13I',raw,0x1f0))==native['depth'] and
            list(struct.unpack_from('<11I',raw,0x1c4))==native['raster'], 'creation native state')
    blend=list(native['blend'])
    if blend[1]==1 and s['color_count']>=2 and p['formats'][1]==42:
        blend[1]=0;blend[11]&=0xffffff00  # native R32_UINT MRT exception, low byte write mask
    require(list(struct.unpack_from('<82I',raw,0x78))==blend, 'creation blend state')
    return True


def validate(d, apis, shaders, source, roots, version='0.3.0'):
    c=d['attachments'];entry=c['entry'];ready=c['ready']
    require(all(c[k] is True for k in ('enabled','described','at_draw','stable','resources_released')), 'incomplete capture/lifetime')
    require(roots and d['root_binding_observed'] is True, 'fresh projection binding missing')
    snapshot(entry,d);snapshot(ready,d)
    for key in ('state','command','attachment','incoming_pso','color_count','flags','views','pipeline'):
        require(entry[key]==ready[key], 'native inputs changed: '+key)
    handles=[v['handle'] for v in ready['views']]
    require(ready['active_attachment']==ready['attachment'] and ready['cached']==handles, 'post-helper handle cache')
    depth=ready['views'][8]
    if depth['owner']:
        require(ready['cached_depth_resource']==depth['resource'], 'depth resource cache')
    require(not ready['incoming_pso'] or ready['incoming_pso']==d['pso_cache'], 'packet PSO selection')
    resources=c['resources']
    require(1<=len(resources)<=9 and len({r['resource'] for r in resources})==len(resources), 'resource capacity/deduplication')
    require({r['resource'] for r in resources}=={v['resource'] for v in ready['views'] if v['owner']}, 'resource associations')
    for r in resources:
        require(r['valid'] is True and uint(r['resource']) and r['resource']>0 and uint(r['identity']) and r['identity']>0 and r['device']==d['device'], 'COM resource/device identity')
        if version=='0.3.0':require(r['resource']==r['identity'], 'legacy COM identity contract')
        require(r['dimension']==3 and all(uint(r[k]) and r[k]>0 for k in ('width','height','depth_or_array','mips','samples','format')) and
                all(uint(r[k]) for k in ('quality','layout','flags','alignment')), 'resource description')
    if version in ('0.3.1','0.4.0','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        checks=c['resource_checks']
        require(len(checks)==3 and [v['phase'] for v in checks]==[0,1,2], 'resource check phases')
        for phase in checks:
            require(len(phase['queries'])==len(resources), 'resource query capacity/completeness')
            for q,r in zip(phase['queries'],resources):
                require(q['valid'] is True and q['temporaries_released'] is True and q['checkpoint']==15, 'resource query incomplete')
                require(vector(q['results'],6) and all(hr<0x80000000 for hr in q['results']), 'resource query HRESULT')
                require(q['candidate']==q['resource']==q['roundtrip']==r['resource'] and q['device']==r['device'], 'resource interface roundtrip/device')
                require(q['identity']==q['identity_repeat']==q['roundtrip_identity']==r['identity'], 'canonical identity changed')
    draw_events=[e for e in apis if e['kind'] in (14,15,16)]
    require(len(draw_events)==1 and d['order']<c['state_order']<c['draw_order']==draw_events[0]['order']<d['exit_order'], 'attachment chronology')
    previous=d['order'];om=[];created=[];barriers=[];pso=[];viewports=[];scissors=[]
    for e in apis:
        k=e['kind']
        require(k in (*range(9,18),20,21,22,23), 'unsupported API/reset')
        require(previous<e['order'], 'overlapping API scope');previous=e['exit_order']
        if k not in (14,15,16):require(e['exit_order']<c['draw_order'], 'late state API')
        if k==20:
            b=e['outputs'];om.append(e)
            require(e['exit_order']<c['state_order'] and b['count']==ready['color_count']==e['args'][0] and
                    b['contiguous']==e['args'][2]==0 and vector(b['colors'],8,64) and b['colors']==handles[:8] and
                    b['depth_present']==bool(depth['owner'])==bool(e['args'][3]) and b['depth']==handles[8], 'OM attachment bind')
        elif k==21:
            rows=e['barriers'];require(1<=len(rows)==e['args'][0]<=64, 'barrier call bounds')
            for b in rows:
                require(all(uint(b[key],32) for key in ('type','flags','subresource','before','after')) and
                        uint(b['resource']) and uint(b['other']) and b['flags']==0 and b['type'] in (0,1,2), 'barrier record/partial transition')
                if b['type']==0:require(b['resource']>0 and b['other']==0, 'transition resource')
                else:require(b['subresource']==b['before']==b['after']==0 and (b['type']==1 or b['other']==0), 'barrier union')
            barriers.extend(rows)
        elif k==22:
            require(c['state_order']<e['order'], 'creation outside state setup');created.append(e)
        elif k==12:pso.append(e)
        elif k==13:
            require(e['args'][0]==1, 'viewport count');vp=floats(e['viewport'],6)
            require(vp==(*map(float,ready['viewport']),0.,1.), 'viewport cache/API');viewports.append(e)
        elif k==23:
            require(e['args'][0]==1 and e['scissor']==ready['scissor'], 'scissor cache/API');scissors.append(e)
    require(len(barriers)==c['barrier_count']<=64, 'total barrier bound')
    if not om:require(entry['cached']==handles, 'missing fresh OM binding')
    if pso:require(pso[-1]['args'][0]==d['pso_cache']!=0, 'actual PSO bind')
    was_created=creation(d['pipeline_creation'],created,d,ready,shaders)
    if was_created:require(pso and created[0]['exit_order']<pso[-1]['order'], 'created PSO not bound')
    depth_desc=next((r for r in resources if r['resource']==depth['resource']),None)
    native=ready['pipeline'];depth_state=native['depth']
    compatible=bool(depth_desc and depth_desc['flags']&2 and depth_desc['samples']==1 and
                    (depth_desc['format'],native['dsv_format']) in ((39,40),(40,40),(44,45),(45,45),(53,55),(55,55),(19,20),(20,20)))
    return dict(attachment_resource_descriptions_correlated=True,attachment_color_count=ready['color_count'],
        depth_resource_present=bool(depth['owner']),depth_resource_description=depth_desc,
        native_depth_enable=depth_state[0],native_depth_write_mask=depth_state[1],native_depth_comparison=depth_state[2],
        depth_write_candidate=compatible and depth_state[0]==1 and depth_state[1]==1 and 1<=depth_state[2]<=8,
        fresh_output_binding_observed=bool(om),fresh_pipeline_binding_observed=bool(pso),
        fresh_viewport_observed=bool(viewports),fresh_scissor_observed=bool(scissors),
        viewport_matches_projection_extent=ready['viewport']==[0,0,*floats(source['viewport_bits'],2)],
        native_pipeline_metadata_correlated=True,actual_pipeline_creation_observed=was_created,
        created_pso_bytecode_correlated=was_created,recorded_barriers=len(barriers),
        view_descriptor_provenance_verified=False,resource_state_at_gpu_execution_verified=False,
        executing_pso_bytecode_verified=False,gpu_contents_verified=False,submission_verified=False,raster_jitter_verified=False,
        **(dict(resource_identity_roundtrip_verified=True,resource_identity_checks=3*len(resources)) if version in ('0.3.1','0.4.0','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else {}))

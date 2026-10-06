"""Qualify CPU native output-copy construction, not GPU consumption or DLSS."""
from temporal_shader import require


def review(header,capture):
    h=capture['final_handoff']
    require(h['schema']==1 and h['policy']=='native-final-copy-observer-v1' and h['active'] is True,'handoff policy')
    require(h['fault']==h['pending']==h['mutations']==0 and h['published']==h['completed']==3,'handoff counts')
    require(h['hooks_disabled'] is True and h['drained_before_disable'] is True and h['pins_acquired']==h['pins_released']==6,'handoff cleanup')
    require(h['target']==header['base']+0x23917a0 and h['expected_caller']==header['base']+0x23931b3,'handoff code identity')
    require(h['gpu_consumption_verified'] is False,'handoff scope')
    require(len(h['records'])==len(capture['records'])==3,'handoff sample count')
    rectangles=[];destinations=[]
    for i,(r,cpu) in enumerate(zip(h['records'],capture['records'])):
        require(r['index']==i and all(r[k] is True for k in ('valid','returned','owners_stable','stamp_stable','rectangle_stable','copy_branch')),'handoff qualification')
        require(all(r[k]==cpu[k] for k in ('frame','epoch','bank','async','view','context','layer','thread')),'handoff TAA association')
        require(r['observed_thread']==r['thread'] and r['caller']==h['expected_caller'] and r['attachment']>0,'handoff call')
        require(0<=r['count_before']<65534 and r['count_after']==r['count_before']+1 and r['array_before']>0 and r['array_after']>0,'handoff queue')
        source,destination=r['source'],r['destination'];selected=cpu['inputs'][4]
        require(all(v==selected[k] for k,v in source.items()),'handoff selected output resource')
        require(source['resource']!=destination['resource'] and source['identity']!=destination['identity'],'handoff resource alias')
        for owner in (source,destination):
            require(all(owner[k]>0 for k in ('owner','resource','identity','backing','type')),'handoff ownership')
            require(owner['dimension']==3 and owner['array_size']==owner['mips']==owner['sample_count']==1 and owner['sample_quality']==0,'handoff texture shape')
            require([owner['engine_width'],owner['engine_height']]==[owner['width'],owner['height']]==cpu['scale_state']['display'],'handoff full-size allocation')
        require(source['engine_format']==destination['engine_format'] and source['format']==destination['format'],'handoff copy formats')
        rect=r['rectangle'];require(len(rect)==4 and 0<=rect[0]<rect[2]<=source['width'] and 0<=rect[1]<rect[3]<=source['height'],'handoff source rectangle')
        require(rect[2]-rect[0]<=destination['width'] and rect[3]-rect[1]<=destination['height'],'handoff destination bounds')
        packet=r['packet']
        require(packet['object']>0 and packet['opcode']==13 and packet['source']==source['owner'] and packet['destination']==destination['owner'],'handoff copy packet')
        require(packet['rectangle']==rect and packet['xy']==[0,0] and all(packet[k]==0 for k in ('mode','source_subresource','destination_subresource','optional')),'handoff packet geometry')
        rectangles.append(rect);destinations.append(destination['resource'])
    return dict(native_final_copy_construction_verified=True,final_copy_rectangles=rectangles,
                final_destination_resources=destinations,final_copy_gpu_consumption_verified=False,
                full_output_handoff_verified=False)

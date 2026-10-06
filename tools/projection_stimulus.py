"""Independent baseline/stimulus/recovery checks; no raster-execution claim."""
from projection_contract import require, floats, matrices


def validate(experiment, selected, armed, publication, header):
    sequence=header['version'] in ('0.9.0','0.9.1');count=5 if sequence else 3;injections=count-2
    if sequence:require(type(experiment.get('draw_waits')) is int and 0<=experiment['draw_waits']<=64,'sequence draw wait bound')
    deferrals=experiment.get('recovery_deferrals',0)
    if header['version'] in ('0.2.1','0.8.3','0.8.4','0.9.0','0.9.1'):
        require(type(experiment.get('recovery_deferrals')) is int and 0<=deferrals<=64, 'recovery deferral bound')
    else:require(deferrals==0, 'unexpected legacy recovery deferrals')
    require(experiment['count']==count and len(experiment['calls'])==count and
            experiment['injections']==experiment['restorations']==injections, 'stimulus budget/completion')
    calls=experiment['calls'];sources=[c['source'] for c in calls]
    first=sources[0];deltas=[]
    for stage,(call,s) in enumerate(zip(calls,sources)):
        require(call['stage']==stage and call['injected'] is (0<stage<count-1) and
                all(call[k] is True for k in ('returned','restored','complete')) and call['restored_bits']==0, 'stimulus restoration')
        require(s['valid'] is True and s['projection_valid'] is True and s['matrix_relation'] is True and
                s['projection_rejection']==0 and s['projection_checkpoint']==100, 'stimulus CPU snapshot')
        require(0<s['bytes']<=4096 and s['bytes']==s['words']*4 and s['constants']==s['tagged_constants'], 'stimulus source extent')
        require(s['order']<call['exit_order'], 'stimulus order')
        if stage:
            previous=sources[stage-1]
            require(all(s[k]==first[k] for k in ('camera','camera_type','thread','renderer','context','view','table','viewport_bits')), 'stimulus anchors changed')
            require(s['frame']>previous['frame'] and s['epoch']>previous['epoch'] and s['order']>calls[stage-1]['exit_order'], 'stimulus frame sequence')
        require(s['tick']>=first['tick'] and s['tick']-first['tick']<header['window_ms'], 'stimulus time window')
        require(call['before']==call['after'], 'stimulus provider changed')
        p=call['before']
        require(p['valid'] is True and p['use'] in (0,1) and p['sizing']&1==0 and p['wrapper']==0 and
                p['backend']==armed['backend'] and p['device']==armed['device'], 'stimulus provider exclusion')
        candidate=call['candidate']
        require(type(candidate) is int and 0<=candidate<2**64, 'stimulus sample bits')
        words=[candidate&0xffffffff,candidate>>32];xy=floats(words,2)
        require(max(map(abs,xy))<=0.5 and any(xy), 'stimulus sample range')
        require(s['table20_bits']==words and s['jitter_bits']==(words if 0<stage<count-1 else [0,0]), 'stimulus sample identity')
        matrices(s)
        jitter=floats(s['jitter_bits'],2);width,height=floats(s['viewport_bits'],2)
        proj=floats(s['projection_bits'],16);plain=floats(s['unjittered_bits'],16)
        require(any(plain[3::4]), 'stimulus degenerate projection')
        for row in range(4):
            for col in range(4):
                i=4*row+col
                delta=plain[4*row+3]*(jitter[0]/width if col==0 else jitter[1]/height if col==1 else 0)
                require(abs((proj[i]-plain[i])-delta)<=8*2**-23*max(1e-6,abs(proj[i]),abs(plain[i]),abs(delta)), 'stimulus projection relation')
        vp=floats(s['view_projection_bits'],16);nvp=floats(s['unjittered_view_projection_bits'],16)
        difference=max(abs(vp[i]-nvp[i]) for i in range(16) if i%4<2);deltas.append(difference)
        if 0<stage<count-1:
            require(difference>0 and proj!=plain, 'no measured nonzero jitter response')
        else:
            require(vp==nvp and proj==plain, 'baseline/recovery not zero')
    if sequence:require(len({c['candidate'] for c in calls[1:-1]})==injections,'sequence repeated jitter sample')
    require({k:v for k,v in sources[1].items() if k!='event'}=={k:v for k,v in selected.items() if k!='event'}, 'bound source is not stimulus')
    require(calls[1]['exit_order']<publication['order'], 'publication precedes restoration')
    return dict(stimulus_cpu_sequence_verified=True,camera_restored=True,injections=injections,restorations=injections,
                cpu_frames=[s['frame'] for s in sources],view_projection_xy_delta_max=deltas,
                recovery_admissions_deferred=deferrals,**(dict(sequence_primary_draw_waits=experiment['draw_waits']) if sequence else {}))

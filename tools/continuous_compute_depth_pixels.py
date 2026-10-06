"""Validate bounded depth-copy identity/lifetime. Pixel interpretation is separate."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import integer


def review(t, resources, previous=None, final=False):
    require(t['schema']==1 and t['policy']=='native-depth-before-reduction-v1'
            and t['diagnostic_only'] is True, 'compute depth policy')
    captures=t['captures']
    require(type(captures) is list and len(captures)==2, 'two bounded depth buffers')
    for i,c in enumerate(captures):
        for k in ('error','list','generation','resource','frame','packet','state','queue','signal','completed',
                  'width','height','format','subresource','row_pitch','allocation_bytes','pixel_bytes'):
            require(integer(c[k]), 'depth counter '+k)
        for k in ('attempted','recorded','reset','saved','released'):require(type(c[k]) is bool, 'depth flag '+k)
        w,h=c['width'],c['height']
        require((w,h)==(2560,1440) and c['format']==41 and c['subresource']==0 and c['row_pitch']>=4*w
                and c['row_pitch']%256==0 and c['allocation_bytes']==c['row_pitch']*(h-1)+4*w, 'depth footprint')
        require(c['error'] in (0,1,2,3,4,10,11,12), 'depth error bounds')
        if c['attempted']:
            require(c['recorded'] and c['state'] in (64,192) and all(c[k] for k in ('list','generation','resource','frame','packet')), 'depth record identity')
            require(i<len(resources['samples']), 'depth sample association')
            sample=resources['samples'][i];p=sample['passes'][0]
            require(not sample['error'] and not p['error'] and p['calls']==1 and p['entered']
                    and (c['list'],c['generation'],c['frame'],c['packet'])==(p['at'][1],p['at'][2],p['entry'][8],p['packet']), 'depth native dispatch association')
            matches=[j for j,u in enumerate(p['uses']) if u[0]&15==2 and u[0]>>24==0]
            require(len(matches)==1, 'one depth SRV t0')
            j=matches[0];r=sample['resources'][p['uses'][j][1]]
            require(r['owner'][3]==c['resource'] and p['states'][j]==[1,c['state']]
                    and r['description'][2:11]==[w,h,1,1,41,1,0,0,1], 'depth resource and observed state')
        else:
            require(not any(c[k] for k in ('recorded','reset','saved','list','generation','resource','frame','packet','state','queue','signal','completed','pixel_bytes')), 'unissued capture has no GPU claim')
        require(bool(c['signal'])==bool(c['queue']) and (not c['signal'] or c['recorded']), 'depth submission')
        require(c['completed'] in (0,c['signal']) and (not c['reset'] or c['signal']), 'depth completion and reset')
        if c['saved']:
            require(c['recorded'] and c['reset'] and c['signal']==c['completed']>0 and not c['error']
                    and c['released'] and c['pixel_bytes']==4*w*h, 'depth save after completion and retirement')
        else:require(c['pixel_bytes']==0, 'unsaved pixels not claimed')
        if c['released'] and c['attempted']:require(c['saved'], 'issued references released after save')
        if final:require(c['released'] and (not c['attempted'] or c['saved']), 'depth references retired at stop')
        if previous:
            old=previous['captures'][i]
            require(all(not old[k] or c[k] for k in ('attempted','recorded','reset','saved','released')), 'depth flag rollback')
            if old['attempted']:
                require(all(c[k]==old[k] for k in ('list','generation','resource','frame','packet','state','width','height','format','subresource','row_pitch','allocation_bytes')), 'depth identity immutable')
            if old['signal']:require((c['signal'],c['queue'])==(old['signal'],old['queue']), 'depth signal immutable')
            if old['completed']:require(c['completed']==old['completed'], 'depth completion rollback')
            if old['error']:require(c['error']==old['error'], 'depth error immutable')
            if old['saved']:require(c==old, 'saved capture immutable')
    return deepcopy(t)

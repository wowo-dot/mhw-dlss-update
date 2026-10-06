"""Verify bounded, history-retaining DLAA at the independent AA-Off input point."""
import argparse
import hashlib
import json
from pathlib import Path
from arm_aa_boundary import parse, require
from review_aa_deferred import review as deferred_review
from review_aa_gpu import review_trace
from review_aa_inputs import review_images
from review_aa_projection import review_bindings, review_projection, review_shutdown
from review_ngx_startup import review as review_startup
from projection_contract import floats
from temporal_inline import pack_reference


def review_sdk_contract(rows):
    g, a = rows[13], rows[16]
    p = a['sdk']; records = a['receipts']; submission = g['submission']
    require(a['schema'] == 1 and a['count'] == 8 and a['finished'] and a['saved'] and not a['failure'], 'SDK closure')
    require(a['native_output_written'] is False and a['private_destination'] > 0 and a['private_destination'] not in g['watched'], 'private SDK destination')
    require(p['schema'] == 1 and not p['failure'] and all(p[k] for k in ('creation_completed','issued','restored','copied','released','continuous','shutdown_deferred')), 'SDK lifetime')
    require(all(p[k] == 1 for k in ('create_result','evaluate_result','release_result','destroy_result')), 'SDK API results')
    require(p['evaluations'] == 8 and p['resets'] == 1 and p['captures'] == 0 and p['reset'] is False, 'retained history')
    require(p['native_output'] == a['private_destination'] and len(p['raw']) == len(set(p['raw'])) == 3 and
            all(v > 0 and v not in g['watched'] and v != a['private_destination'] for v in p['raw']), 'private SDK resources')
    require([p[k] for k in ('create_flags','mode','preset','input_formats','converted_formats','mv_scale','pre_exposure','mv_jittered')] ==
            [75,5,11,[26,41,42],[2,41,16],[1,1],1,False], 'SDK configuration')
    require(p['width'] == rows[2]['width'] and p['height'] == rows[2]['height'] and p['device'] == rows[2]['device'], 'SDK device/extent')
    require(abs(p['frame_ms']-16.66666603088379) < 1e-6 and 0 < p['allocation_bytes'] <= 419430400, 'SDK diagnostic timing/budget')
    require(len(records) == 8, 'SDK receipt count')
    for i,(r,s,b) in enumerate(zip(records,submission['samples'],g['records'])):
        require(r['receipt'] == i and r['frame'] == s['frame'] == b['frame'] and r['epoch'] == b['projection_epoch'] and
                r['generation'] == s['reset'] and r['jitter_bits'] == b['jitter_bits'], 'SDK source identity')
        require(all(r[k] for k in ('attempted','previous_completed','inputs_restored','bindings_restored','success')), 'SDK recording completion')
        x,y = floats(r['jitter_bits'],2)
        require(len(r['sdk_jitter']) == 2 and all(abs(a-b)<1e-8 for a,b in zip(r['sdk_jitter'],(x/2,-y/2))), 'SDK raster jitter mapping')
        event = submission['events'][b['read_token']]
        require(event['enter_tick'] <= r['begin_tick'] <= r['end_tick'] <= event['exit_tick'], 'SDK inside native read-point lease')
        if i:
            previous=submission['samples'][i-1]
            retirement=next(e for e in submission['events'] if e['enter']==previous['retire'])
            require(previous['completed_tick'] <= r['begin_tick'] and retirement['exit_tick'] <= r['begin_tick'], 'SDK GPU history serialization')
            require(r['frame'] == records[i-1]['frame']+1 and r['epoch'] == records[i-1]['epoch']+1, 'SDK adjacent history')
    last=records[-1];s=submission['samples'][-1]
    require(p['frame']==last['frame'] and p['epoch']==last['epoch'] and p['generation']==last['generation'] and p['command']==s['command'], 'last SDK identity')
    require(p['completion_fence']==s['fence'] and p['completion_value']==1 and p['jitter']==last['sdk_jitter'], 'SDK retirement fence/jitter')
    return dict(independent_sdk_evaluations=8,sdk_history_retained_verified=True,native_binding_restoration_tested=True,
                native_output_written=False,independent_dlaa_presentation_verified=False,sdk_temporal_quality_verified=False)


def review_sdk_pixels(a, native, directory, width, height):
    import numpy as np
    images=a['images'];require(len(images)==15,'SDK image count');folder=Path(directory)
    entries={};names=set();pixels=width*height
    for entry in images:
        key=entry['receipt'],entry['role'];require(key not in entries and key[0] in (0,3,7) and 0 <= key[1] < 5,'SDK image identity')
        role=key[1];name=entry['file'];require(name and Path(name).name==name and name not in names,'SDK local image name');names.add(name)
        require(entry['copied'] and entry['saved'] and entry['format']==(2,41,16,2,26)[role], 'SDK image metadata')
        data=(folder/name).read_bytes();require(len(data)==entry['bytes']==pixels*(16,4,8,16,4)[role] and hashlib.sha256(data).hexdigest()==entry['sha256'],'SDK image hash/size')
        entries[key]=np.frombuffer(data,dtype='<u4' if role==4 else '<f4')
    source={(v['receipt'],v['role']):v for v in native['images']};differences=[]
    for receipt in (0,3,7):
        raw=[np.fromfile(folder/source[receipt,i]['file'],dtype='<u4') for i in range(3)]
        color=entries[receipt,0].reshape(-1,4);depth=entries[receipt,1];motion=entries[receipt,2].reshape(-1,2)
        for channel,(shift,bits) in enumerate(((0,6),(11,6),(22,5))):
            part=raw[0]>>shift;mantissa=part&((1<<bits)-1);exponent=(part>>bits)&31
            expected=np.where(exponent==0,np.ldexp(mantissa.astype('float32'),1-15-bits),
                              np.ldexp(1+mantissa.astype('float32')/(1<<bits),exponent.astype('int32')-15))
            require(np.all(exponent!=31) and np.array_equal(color[:,channel],expected),'exact native color conversion')
        require(np.all(color[:,3]==1) and np.array_equal(depth.view('<u4'),raw[1]),'exact alpha/depth conversion')
        dx=(raw[2]>>16).astype('<u2').view('<f2').astype('float32');dy=(raw[2]&0xfffe).astype('<u2').view('<f2').astype('float32')
        require(np.array_equal(motion[:,0],-dx*np.float32(width/2)) and np.array_equal(motion[:,1],dy*np.float32(height/2)),'exact native motion conversion')
        rgb=entries[receipt,3].reshape(-1,4)[:,:3]
        require(all(np.isfinite(v).all() for v in (color,depth,motion,rgb)) and not np.any(rgb==65504),'finite written SDK pixels')
        require(np.array_equal(entries[receipt,4],pack_reference(rgb)),'exact packed DLAA output')
        differences.append(int(np.count_nonzero(rgb!=color[:,:3])))
    return dict(sdk_images_verified=15,exact_converted_input_pixels=3*pixels,packed_sdk_pixels_verified=3*pixels,
                sdk_changed_rgb_values=differences,independent_dlaa_private_output_verified=True)


def review(raw,dll_sha,directory):
    result=deferred_review(raw,dll_sha,projection=True,sdk=True);rows=parse(raw,sdk=True)
    result.update(review_trace(rows[13],rows[12]['requests'],read_points=True))
    result.update(review_bindings(rows[13]));result.update(review_projection(rows))
    result.update(review_shutdown(rows[13],rows[-1]['first_disable_tick']))
    result.update(review_images(rows[14],directory));result.update(review_sdk_contract(rows))
    startup_path=Path(directory)/f"startup-{rows[0]['pid']}-{rows[0]['creation']}.jsonl"
    startup_raw=startup_path.read_bytes();require(0<len(startup_raw)<=65536,'startup log bound')
    startup=[json.loads(line) for line in startup_raw.decode().splitlines()];accepted=review_startup(startup)
    require(accepted['accepted'] and all(startup[0][k]==rows[0][k] for k in ('pid','creation','base','version','host_sha256','dll_sha256')),'startup identity')
    entered=next(r for r in startup if r['event']=='native_enter')
    require(entered['device']==rows[2]['device'] and entered['tick']<rows[13]['submission']['started_tick'],'startup device/timing')
    require(accepted['adapter_luid']==[rows[2]['adapter_high'],rows[2]['adapter_low']] and all(accepted[k]==rows[2][k] for k in ('width','height','swapchain_format')),'startup graphics')
    result.update(startup_ownership_verified=True,startup_log_sha256=hashlib.sha256(startup_raw).hexdigest())
    result.update(review_sdk_pixels(rows[16],rows[14],directory,rows[2]['width'],rows[2]['height']))
    return result


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--log',required=True,type=Path);ap.add_argument('--dll-sha256',required=True);ap.add_argument('--output',required=True,type=Path)
    args=ap.parse_args();require(not args.output.exists() and args.log.stat().st_size<=4194304,'artifact bounds')
    result=review(args.log.read_bytes(),args.dll_sha256,args.log.resolve().parent)
    with args.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2))

"""Verify independent input copies, state restoration and completed readback files."""
import argparse
import hashlib
import json
from pathlib import Path
from arm_aa_boundary import parse, require
from review_aa_deferred import review as deferred_review
from review_aa_gpu import review_trace


def review_images(report, directory):
    require(report['event'] == 'input_readbacks' and report['prepared'] and report['gpu_safe'] and report['released'], 'readback lifetime')
    images = report['images']
    require(len(images) == 9, 'nine input images')
    seen = set()
    for image, (receipt, role) in zip(images, ((i,j) for i in (0,3,7) for j in range(3))):
        require(image['receipt'] == receipt and image['role'] == role, 'readback identity')
        require(all(image[k] for k in ('attempted','transitioned','copied','restored','saved')) and image['map_result'] == 0, 'copy and save completion')
        require(image['state_before'] == image['state_after'] == 128 and image['copy_state'] == 2048, 'native state restoration')
        require(image['format'] == (26,41,42)[role] and image['row_pitch'] == 10240 and image['bytes'] == 14745600, 'readback footprint')
        name = image['file']
        require(name and name not in seen and '/' not in name and '\\' not in name and ':' not in name and name.endswith(f'-{receipt}-{role}.bin'), 'local image filename')
        seen.add(name)
        path = directory / name
        require(path.is_file() and not path.is_symlink() and path.stat().st_size == image['bytes'], 'image extent')
        require(hashlib.sha256(path.read_bytes()).hexdigest() == image['sha256'].lower(), 'image checksum')
    return dict(readback_images_verified=9,readback_receipts=[0,3,7],native_resource_states_restored=True,readback_references_released=True)


def review(raw, dll_sha, directory):
    result = deferred_review(raw,dll_sha,pixels=True)
    rows = parse(raw,pixels=True)
    result.update(review_trace(rows[13],rows[12]['requests'],read_points=True))
    result.update(review_images(rows[14],directory))
    result['read_point'] = 'first-paired-transition-after-marker'
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--dll-sha256',required=True)
    ap.add_argument('--output',type=Path,required=True)
    a = ap.parse_args()
    require(not a.output.exists() and a.log.stat().st_size <= 4194304,'artifact bounds')
    result = review(a.log.read_bytes(),a.dll_sha256,a.log.resolve().parent)
    with a.output.open('x') as out: json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2))

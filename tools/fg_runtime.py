"""Assemble the shared Streamline + NGX runtime folder for frame-generation builds.

  python -B tools/fg_runtime.py --streamline <SL bin/x64> --ngx <NGX rel dir> --out <new folder>

With DLSS-G loaded, Streamline initializes NGX before the game does, and NGX
keeps the feature search path of that first initialization. The game's DLSS
provider must therefore sit in the same folder as the Streamline plugins.
Every file is copied from a pinned source and verified by SHA-256.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

STREAMLINE = {
    'sl.interposer.dll': '2a79db6857ae8c75bbd871a9489c48bc6a39f7fcc88b9b02afd53d0376cbec66',
    'sl.common.dll': 'c57930ef5a8a3fe9be85efdf71a61d8107c1148e8a6aed456464547128f7f4ae',
    'sl.pcl.dll': '699ab461e64e95189a7fe6a21c79ad237cf56b60ea748cb6c840cd5431ba91d1',
    'sl.reflex.dll': '7e6e4ccc4b561bd449fb0da90709d9b96b08c3f6f4697362caaa359e72a58a67',
    'sl.dlss_g.dll': '1fec3f8fdfc59d78c4445c276c1a0fb798bf251985f348597dc2b44d0c995e52',
    'nvngx_dlssg.dll': '135eaf0733c1e37381a8c28abcf7a862404a54132b81787c04e35d09efc5e36f',
}
NGX = {'nvngx_dlss.dll': 'be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e'}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--streamline', type=Path, required=True)
    p.add_argument('--ngx', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    manifest = []
    for source, pins in ((a.streamline, STREAMLINE), (a.ngx, NGX)):
        for name, digest in pins.items():
            if sha(source / name) != digest:
                raise SystemExit(f'pinned hash mismatch: {source / name}')
            shutil.copyfile(source / name, a.out / name)
            if sha(a.out / name) != digest:
                raise SystemExit(f'copy mismatch: {name}')
            manifest.append(dict(path=name, source=str(source.resolve()), sha256=digest))
    with (a.out / 'runtime-manifest.json').open('x', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
    print(json.dumps(dict(out=str(a.out.resolve()), files=len(manifest)), indent=1))


if __name__ == '__main__':
    main()

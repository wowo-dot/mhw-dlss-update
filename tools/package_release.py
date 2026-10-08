"""Assemble a release package of the alpha DLL with the NVIDIA runtime it was tested with.

  python -B tools/package_release.py --dll <built mhw_dlss_alpha.dll> --version X.Y.Z
      --streamline-sdk <Streamline SDK root> --dlss-sdk <DLSS SDK root> --out <folder>

Writes <out>/mhw-dlss-<version>/ and <out>/mhw-dlss-<version>.zip:

  mhw_dlss.dll, mhw_dlss.ini        the mod and its default configuration
  sl.*.dll, nvngx_dlss*.dll         NVIDIA runtime, unmodified, verified by SHA-256
  licenses/                         NVIDIA's license files for those DLLs
  README.txt, THIRD-PARTY-NOTICES.txt, manifest.json

NVIDIA files are copied only when they match the pinned hashes of the tested
runtime (fg_runtime.py), and only into release packages: never into git
(AGENTS.md). The output folder must not exist yet; nothing is overwritten.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile
from pathlib import Path

from fg_runtime import NGX, STREAMLINE

# License files shipped with each NVIDIA source, by name inside the package.
LICENSES = {
    'streamline': [('license.txt', 'Streamline-MIT-LICENSE.txt'),
                   ('bin/x64/nvngx_dlss.license.txt', 'NVIDIA-RTX-SDKs-LICENSE-Streamline.txt'),
                   ('bin/x64/reflex.license.txt', 'NVIDIA-Reflex-SDK-LICENSE.txt')],
    'dlss': [('LICENSE.txt', 'NVIDIA-RTX-SDKs-LICENSE-DLSS.txt')],
}
COVERAGE = [
    ('sl.interposer.dll, sl.common.dll, sl.pcl.dll, sl.reflex.dll, sl.dlss_g.dll', 'NVIDIA Streamline',
     'Streamline-MIT-LICENSE.txt; Reflex components: NVIDIA-Reflex-SDK-LICENSE.txt'),
    ('nvngx_dlssg.dll', 'NVIDIA DLSS Frame Generation', 'NVIDIA-RTX-SDKs-LICENSE-Streamline.txt'),
    ('nvngx_dlss.dll', 'NVIDIA DLSS Super Resolution', 'NVIDIA-RTX-SDKs-LICENSE-DLSS.txt'),
]
INI = """# MHW DLSS / Frame Generation (alpha) configuration.
# Lines are "key = value"; '#' or ';' starts a comment. Folders are relative to this file.
frame_generation = 2x      ; off, 2x, 3x, 4x, 6x (3x and above need an RTX 50 series GPU)
reflex = on                ; on (off is not available yet)
runtime_dir = .            ; folder with the NVIDIA DLLs shipped in this package
log_dir = logs             ; one log file per launch
"""
README = """MHW DLSS / Frame Generation {version} (alpha)

An open-source mod that adds NVIDIA DLSS Super Resolution, DLSS Frame
Generation and NVIDIA Reflex to Monster Hunter: World (DirectX 12).

Requirements
- Monster Hunter: World on Steam, DirectX 12 enabled. In-game anti-aliasing
  and the game's own DLSS option off.
- An NVIDIA GeForce RTX GPU. Frame Generation needs an RTX 40 or 50 series
  GPU; 3x and above need an RTX 50 series GPU. On other hardware the mod
  turns itself off and the game runs normally.

Configuration: mhw_dlss.ini (comments in the file). DLSS currently runs at
Performance quality (half resolution); other modes come with later builds.

Logs: the logs folder next to mhw_dlss.dll, one file per launch. Please
include it when reporting a problem.

Uninstall: remove every file of this package.

Credits and licenses
This mod uses NVIDIA DLSS, NVIDIA DLSS Frame Generation, NVIDIA Streamline
and NVIDIA Reflex. It is not affiliated with, sponsored or endorsed by NVIDIA.
The NVIDIA DLLs in this package are unmodified and distributed under NVIDIA's
licenses in the licenses folder (see THIRD-PARTY-NOTICES.txt); they are not
covered by the mod's own open-source license.
Monster Hunter: World is a trademark of CAPCOM CO., LTD. This mod is not
affiliated with Capcom.
"""


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(ok, why):
    if not ok:
        raise SystemExit(f'refused: {why}')


def git_identity(repo):
    def run(*args):
        return subprocess.run(['git', '-C', str(repo), *args], capture_output=True, text=True, check=True).stdout.strip()
    return run('rev-parse', 'HEAD'), bool(run('status', '--porcelain'))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--dll', type=Path, required=True)
    p.add_argument('--version', required=True)
    p.add_argument('--streamline-sdk', type=Path, required=True)
    p.add_argument('--dlss-sdk', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--allow-dirty', action='store_true', help='package from a working tree with uncommitted changes')
    a = p.parse_args()
    marker = ('"version":"%s","build":"alpha"' % a.version).encode()
    require(a.dll.is_file() and marker in a.dll.read_bytes(), f'{a.dll} is not the alpha build of version {a.version}')
    commit, dirty = git_identity(Path(__file__).resolve().parents[1])
    require(a.allow_dirty or not dirty, 'the working tree has uncommitted changes')
    name = f'mhw-dlss-{a.version}'
    folder, archive = a.out / name, a.out / f'{name}.zip'
    require(not folder.exists() and not archive.exists(), f'{folder} or its zip already exists')

    sources = [(a.streamline_sdk / 'bin' / 'x64', STREAMLINE), (a.dlss_sdk / 'lib' / 'Windows_x86_64' / 'rel', NGX)]
    for root, pins in sources:
        for file, digest in pins.items():
            require((root / file).is_file() and sha(root / file) == digest, f'{root / file} does not match the tested runtime')
    licenses = [(a.streamline_sdk / src, dst) for src, dst in LICENSES['streamline']] + \
               [(a.dlss_sdk / src, dst) for src, dst in LICENSES['dlss']]
    for src, _ in licenses:
        require(src.is_file(), f'license file {src} is missing')

    (folder / 'licenses').mkdir(parents=True)
    shutil.copyfile(a.dll, folder / 'mhw_dlss.dll')
    (folder / 'mhw_dlss.ini').write_text(INI, encoding='utf-8', newline='\r\n')
    for root, pins in sources:
        for file in pins:
            shutil.copyfile(root / file, folder / file)
    for src, dst in licenses:
        shutil.copyfile(src, folder / 'licenses' / dst)
    (folder / 'README.txt').write_text(README.format(version=a.version), encoding='utf-8', newline='\r\n')
    notices = ['Third-party components (unmodified NVIDIA binaries)', '']
    for files, product, license_file in COVERAGE:
        notices += [f'{product}: {files}', f'  License: licenses/{license_file}', '']
    notices += ['NVIDIA, GeForce RTX, DLSS and Reflex are trademarks of NVIDIA Corporation.',
                'This mod is not affiliated with, sponsored or endorsed by NVIDIA.']
    (folder / 'THIRD-PARTY-NOTICES.txt').write_text('\n'.join(notices) + '\n', encoding='utf-8', newline='\r\n')
    files = sorted(f for f in folder.rglob('*') if f.is_file())
    manifest = dict(schema=1, version=a.version, commit=commit, dirty=dirty,
                    files=[dict(path=f.relative_to(folder).as_posix(), size=f.stat().st_size, sha256=sha(f)) for f in files])
    (folder / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(x for x in folder.rglob('*') if x.is_file()):
            z.write(f, f'{name}/{f.relative_to(folder).as_posix()}')
    print(json.dumps(dict(package=str(folder), zip=str(archive), zip_sha256=sha(archive), files=len(files) + 1, commit=commit)))


if __name__ == '__main__':
    main()

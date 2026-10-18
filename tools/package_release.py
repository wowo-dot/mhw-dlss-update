"""Assemble a release package of the alpha DLL with the NVIDIA runtime it was tested with.

  python -B tools/package_release.py --dll <built mhw_dlss.dll> --version X.Y.Z
      --streamline-sdk <Streamline SDK root> --dlss-sdk <DLSS SDK root> --out <folder>

Writes <out>/mhw-dlss-<version>/ and <out>/mhw-dlss-<version>.zip, laid out as
the game folder receives it: one Astera Loader plugin folder (Astera Loader is
required and loads only mhw_dlss.dll from it). The zip has AsteraLoader/ at its
top, as Astera Loader's and CMFM's do, so Mod Organizer 2 installs it as it is:

  AsteraLoader/plugins/mhw_dlss/mhw_dlss.dll          the mod (an Astera Loader plugin)
  AsteraLoader/plugins/mhw_dlss/mhw_dlss.ini          default configuration
  AsteraLoader/plugins/mhw_dlss/sl.*.dll, nvngx_*     NVIDIA runtime, unmodified, verified by SHA-256
  AsteraLoader/plugins/mhw_dlss/licenses/             NVIDIA's license files for those DLLs
  AsteraLoader/plugins/mhw_dlss/README.txt, LICENSE.txt, THIRD-PARTY-NOTICES.txt, manifest.json

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
# Third-party code compiled into mhw_dlss.dll (its link map lists each library):
# component, license file in this repository (None: one of NVIDIA's above), name
# in the package's licenses folder.
BUILT_IN = [
    ('DxilHash from Microsoft DirectXShaderCompiler (shader container checksum)', 'third_party/dxc_hash/LICENSE.TXT',
     'DirectXShaderCompiler-NCSA-LICENSE.txt'),
    ('NVIDIA DLSS SDK NGX library (nvsdk_ngx_d.lib)', None, 'NVIDIA-RTX-SDKs-LICENSE-DLSS.txt'),
    ('NVIDIA Streamline SDK headers', None, 'Streamline-MIT-LICENSE.txt'),
    ('NVIDIA NVAPI declarations (Reflex sleep status)', 'third_party/nvapi-sleep-status-LICENSE.txt', 'NVAPI-MIT-LICENSE.txt'),
    ('Astera Loader plugin header', 'third_party/al/LICENSE', 'AsteraLoader-MIT-LICENSE.txt'),
    ('CMFM menu header', 'third_party/cmfm/LICENSE', 'CMFM-MIT-LICENSE.txt'),
]
INI = """# MHW DLSS / Frame Generation (alpha) configuration.
# Lines are "key = value"; '#' or ';' starts a comment. Folders are relative to this folder.
dlss_mode = off            ; off, dlaa, quality, balanced, performance (off: the game's own anti-aliasing and resolution)
frame_generation = off     ; off, 2x, 3x, 4x, 6x, dynamic (3x and above and dynamic need an RTX 50 series GPU)
dynamic_target = display   ; dynamic only: display (its refresh rate) or a frame rate from 30 to 1000
frame_generation_auto = off ; on: 2x to 6x pause by themselves when the game would run faster without them
reflex = off            ; on, boost, off (off only while DLSS Frame Generation is not loaded)
frame_generation_plugin = auto ; auto: DLSS Frame Generation loads where the GPU supports it; off: never
runtime_dir = .            ; folder with the NVIDIA DLLs shipped in this package
log_dir = logs             ; one log file per launch
"""
README = """MHW DLSS / Frame Generation {version} (alpha)

An open-source mod that adds NVIDIA DLSS Super Resolution, DLSS Frame
Generation and NVIDIA Reflex to Monster Hunter: World (DirectX 12).

Requirements
- Monster Hunter: World on Steam, DirectX 12 enabled. While DLSS runs, the
  mod sets the game's Anti-Aliasing and Resolution Scaling options as the
  game's own DLSS did (shown greyed in its graphics settings); your choices
  return when DLSS is Off. The game's own NVIDIA DLSS (DLSS 1) is unavailable
  while the mod is installed; if it was on, the game says once that NVIDIA
  DLSS has been switched off.
- An NVIDIA GeForce RTX GPU. Frame Generation needs an RTX 40 or 50 series
  GPU; 3x and above need an RTX 50 series GPU, and Dynamic also needs NVIDIA
  driver 595.41 or newer. On other hardware the mod turns itself off and the
  game runs normally.
- The Microsoft Visual C++ 2015-2022 Redistributable (x64), which most MHW
  plugin setups already have.
- Astera Loader 0.2.2 or newer, which loads this mod
  (https://github.com/wowo-dot/AsteraLoader/releases). Start the game the way
  its README says: with Mod Organizer 2, through its AsteraLoader.exe entry;
  otherwise, with Steam running, start AsteraLoader.exe (or a shortcut to
  it). For Steam's Play button to start it too, set the game's launch
  options in Steam to "<game folder>\\AsteraLoader.exe" %command%.

Install: copy the AsteraLoader folder from this archive into the game folder
(next to MonsterHunterWorld.exe), or install the archive as a mod in Mod
Organizer 2. The mod is an Astera Loader plugin and lives in
AsteraLoader/plugins/mhw_dlss.

Configuration: AsteraLoader/plugins/mhw_dlss/mhw_dlss.ini (comments in the file).
DLSS mode (dlss_mode): DLAA renders at full resolution; Quality, Balanced and
Performance render at 67%, 58% and 50% and upscale. The package starts with
DLSS, frame generation and Reflex off; turn them on in the menu or here.
Off hands the game back: its own anti-aliasing, resolution and other options
work as without the mod, and frame generation pauses until DLSS is back on.
Frame generation (frame_generation): Off, a fixed multiplier (2x to 6x), or
Dynamic, which picks the count itself to reach a target frame rate
(dynamic_target: the display's refresh rate by default).
frame_generation_auto = on lets NVIDIA's Auto mode pause a fixed multiplier
when the game would run faster without it. DLSS Frame Generation loads at
every start on a GPU that supports it, so it can be turned on and off while
playing; frame_generation_plugin = off keeps it unloaded.
Reflex (reflex): On, Boost, or Off. Off works only while DLSS Frame Generation
is not loaded (frame_generation_plugin = off, or a GPU without it): with it
loaded, Reflex keeps the frame pacing smooth.
Edits to the file apply from the next start; the menu (below) applies its
changes at once.
With CMFM installed (an optional Astera Loader plugin, 0.1.0 or newer:
https://github.com/wowo-dot/CMFM/releases), the game's own menus
gain a Mods entry below Options. There, DLSS > Settings changes these
settings, and DLSS > Status shows what the mod is doing.
The menu saves its changes to mhw_dlss.ini.

Logs: AsteraLoader/plugins/mhw_dlss/logs, one file per launch. Please include it when
reporting a problem.
If the mod turns itself off before it can log, the reason is in
AsteraLoader/plugins/mhw_dlss/startup-error.jsonl (with Mod Organizer 2: in its
overwrite folder), and Astera Loader shows it as the plugin's status.

Uninstall: delete the AsteraLoader/plugins/mhw_dlss folder.

Credits and licenses
This mod uses NVIDIA DLSS, NVIDIA DLSS Frame Generation, NVIDIA Streamline
and NVIDIA Reflex. It is not affiliated with, sponsored or endorsed by NVIDIA.
The NVIDIA DLLs in this package are unmodified and distributed under NVIDIA's
licenses in the licenses folder (see THIRD-PARTY-NOTICES.txt); they are not
covered by the mod's own license.
The mod itself is open source under the MIT License (LICENSE.txt).
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
    repo = Path(__file__).resolve().parents[1]
    commit, dirty = git_identity(repo)
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
    licenses += [(repo / src, dst) for _, src, dst in BUILT_IN if src]
    require((repo / 'LICENSE').is_file(), "the mod's LICENSE is missing")
    for src, _ in licenses:
        require(src.is_file(), f'license file {src} is missing')

    data = folder / 'AsteraLoader' / 'plugins' / 'mhw_dlss'
    (data / 'licenses').mkdir(parents=True)
    shutil.copyfile(a.dll, data / 'mhw_dlss.dll')
    (data / 'mhw_dlss.ini').write_text(INI, encoding='utf-8', newline='\r\n')
    for root, pins in sources:
        for file in pins:
            shutil.copyfile(root / file, data / file)
    for src, dst in licenses:
        shutil.copyfile(src, data / 'licenses' / dst)
    (data / 'README.txt').write_text(README.format(version=a.version), encoding='utf-8', newline='\r\n')
    shutil.copyfile(repo / 'LICENSE', data / 'LICENSE.txt')
    notices = ['Third-party components compiled into mhw_dlss.dll', '']
    for component, _, license_file in BUILT_IN:
        notices += [component, f'  License: licenses/{license_file}', '']
    notices += ['Third-party components (unmodified NVIDIA binaries)', '']
    for files, product, license_file in COVERAGE:
        notices += [f'{product}: {files}', f'  License: licenses/{license_file}', '']
    notices += ['NVIDIA, GeForce RTX, DLSS and Reflex are trademarks of NVIDIA Corporation.',
                'This mod is not affiliated with, sponsored or endorsed by NVIDIA.']
    (data / 'THIRD-PARTY-NOTICES.txt').write_text('\n'.join(notices) + '\n', encoding='utf-8', newline='\r\n')
    files = sorted(f for f in folder.rglob('*') if f.is_file())
    manifest = dict(schema=1, version=a.version, commit=commit, dirty=dirty,
                    files=[dict(path=f.relative_to(folder).as_posix(), size=f.stat().st_size, sha256=sha(f)) for f in files])
    (data / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(x for x in folder.rglob('*') if x.is_file()):
            z.write(f, f.relative_to(folder).as_posix())
    print(json.dumps(dict(package=str(folder), zip=str(archive), zip_sha256=sha(archive), files=len(files) + 1, commit=commit)))


if __name__ == '__main__':
    main()

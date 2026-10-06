"""Install or restore a test DLL in the MO2 mod slot with exact backups.

  install --slot <slot dll> --dll <built dll> --run <new run folder>
          [--runtime-dir <Streamline runtime>] [--seed-from <previous run folder>]
          [--ngx-runtime-dir <NGX runtime written to sdk-runtime-dir.txt>]
  restore --slot <slot dll> --run <run folder>

The slot path and runtime directory are machine-local arguments, never
defaults. Refuses while MonsterHunterWorld.exe runs. The run folder receives the
previous slot DLL and sidecars byte-for-byte, a receipt with hashes and the git
commit, and restore instructions. The game directory is never written.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from evidence import game_running

SEEDS = ('sdk-runtime-dir.txt', 'temporal-native-gates.json')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def slot_files(slot):
    """The slot DLL and the sidecars the probe reads next to it."""
    return {'dll': slot, 'output-dir': Path(str(slot) + '.output-dir'),
            'runtime-dir': Path(str(slot) + '.runtime-dir'), 'mode': slot.with_suffix('.mode')}


def git_identity(repo):
    def run(*args):
        return subprocess.run(['git', '-C', str(repo), *args], capture_output=True, text=True, check=True).stdout.strip()
    return dict(commit=run('rev-parse', 'HEAD'), dirty=bool(run('status', '--porcelain')))


def install(a):
    if game_running():
        raise SystemExit('close MonsterHunterWorld.exe first')
    slot, run = a.slot.resolve(), a.run.resolve()
    run.mkdir(parents=True, exist_ok=False)
    backup = run / 'previous-slot'
    backup.mkdir()
    files = slot_files(slot)
    previous = {}
    for key, path in files.items():
        if path.exists():
            shutil.copyfile(path, backup / path.name)
            previous[key] = dict(path=str(path), sha256=sha(path))
    if a.seed_from:
        for name in SEEDS:
            shutil.copyfile(a.seed_from / name, run / name)
    if a.ngx_runtime_dir:  # Overrides the seeded NGX runtime folder.
        (run / 'sdk-runtime-dir.txt').write_bytes(str(a.ngx_runtime_dir.resolve()).encode('utf-8'))
    shutil.copyfile(a.dll, run / a.dll.name)
    receipt = dict(schema=1, installed_utc=datetime.now(timezone.utc).isoformat(), slot=str(slot), dll=a.dll.name,
                   dll_sha256=sha(a.dll), previous=previous, git=git_identity(Path(__file__).resolve().parents[1]),
                   output_dir=str(run), runtime_dir=str(a.runtime_dir.resolve()) if a.runtime_dir else None,
                   ngx_runtime_dir=str(a.ngx_runtime_dir.resolve()) if a.ngx_runtime_dir else None,
                   restore=f'python -B tools/mo2_slot.py restore --slot "{slot}" --run "{run}"')
    shutil.copyfile(a.dll, slot)
    files['output-dir'].write_bytes((str(run) + '\n').encode('utf-8'))
    if a.runtime_dir:
        files['runtime-dir'].write_bytes((str(a.runtime_dir.resolve()) + '\n').encode('utf-8'))
    if sha(slot) != receipt['dll_sha256']:
        raise SystemExit('installed DLL does not match')
    with (run / 'install-receipt.json').open('x', encoding='utf-8') as f:
        json.dump(receipt, f, indent=2)
    return receipt


def restore(a):
    if game_running():
        raise SystemExit('close MonsterHunterWorld.exe first')
    slot, run = a.slot.resolve(), a.run.resolve()
    receipt = json.loads((run / 'install-receipt.json').read_bytes())
    restored = []
    for key, item in receipt['previous'].items():
        source = run / 'previous-slot' / Path(item['path']).name
        if sha(source) != item['sha256']:
            raise SystemExit(f'backup changed: {source}')
        shutil.copyfile(source, item['path'])
        restored.append(item['path'])
    return dict(restored=restored, slot_sha256=sha(slot))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=('install', 'restore'))
    p.add_argument('--slot', type=Path, required=True)
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--dll', type=Path)
    p.add_argument('--runtime-dir', type=Path)
    p.add_argument('--seed-from', type=Path)
    p.add_argument('--ngx-runtime-dir', type=Path)
    a = p.parse_args()
    if a.action == 'install' and not a.dll:
        p.error('install needs --dll')
    print(json.dumps(install(a) if a.action == 'install' else restore(a), indent=2))


if __name__ == '__main__':
    main()

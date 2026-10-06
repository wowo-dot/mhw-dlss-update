"""Seal a finished runtime evidence folder with a hash manifest.

Usage: python -B tools/evidence.py seal <run-folder>

Refuses while MonsterHunterWorld.exe is running or if the folder is already
sealed. The manifest lists every file (path, size, SHA-256); later edits to a
sealed folder are detectable with `verify`.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path

MANIFEST = 'archive-manifest.json'


def game_running():
    k = c.WinDLL('kernel32', use_last_error=True)

    class Entry(c.Structure):
        _fields_ = [('dwSize', w.DWORD), ('cntUsage', w.DWORD), ('pid', w.DWORD), ('heap', c.c_size_t),
                    ('module', w.DWORD), ('threads', w.DWORD), ('parent', w.DWORD), ('priority', w.LONG),
                    ('flags', w.DWORD), ('exe', w.WCHAR * 260)]
    k.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]; k.CreateToolhelp32Snapshot.restype = w.HANDLE
    k.Process32FirstW.argtypes = [w.HANDLE, c.POINTER(Entry)]; k.Process32NextW.argtypes = [w.HANDLE, c.POINTER(Entry)]
    k.CloseHandle.argtypes = [w.HANDLE]
    h = k.CreateToolhelp32Snapshot(2, 0)
    if not h or h == c.c_void_p(-1).value:
        raise RuntimeError('process snapshot')
    try:
        e = Entry(); e.dwSize = c.sizeof(e); ok = k.Process32FirstW(h, c.byref(e))
        while ok:
            if e.exe.lower() == 'monsterhunterworld.exe':
                return True
            ok = k.Process32NextW(h, c.byref(e))
    finally:
        k.CloseHandle(h)
    return False


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def listing(folder):
    return [dict(path=p.relative_to(folder).as_posix(), bytes=p.stat().st_size, sha256=sha256(p))
            for p in sorted(folder.rglob('*')) if p.is_file() and p.name != MANIFEST]


def seal(folder):
    if game_running():
        raise SystemExit('MonsterHunterWorld.exe is running; seal only after normal exit')
    target = folder / MANIFEST
    rows = listing(folder)
    with target.open('x', encoding='utf-8') as stream:
        json.dump(rows, stream, indent=2)
    return dict(sealed=str(folder), files=len(rows), manifest_sha256=sha256(target))


def verify(folder):
    rows = json.loads((folder / MANIFEST).read_bytes())
    current = listing(folder)
    if current != rows:
        changed = sorted({r['path'] for r in rows} ^ {r['path'] for r in current} |
                         {a['path'] for a, b in zip(rows, current) if a != b})
        raise SystemExit(f'evidence changed: {changed[:10]}')
    return dict(verified=str(folder), files=len(rows), manifest_sha256=sha256(folder / MANIFEST))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=('seal', 'verify'))
    p.add_argument('folder', type=Path)
    a = p.parse_args()
    print(json.dumps((seal if a.action == 'seal' else verify)(a.folder.resolve())))


if __name__ == '__main__':
    main()

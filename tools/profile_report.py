"""Stack-sampler report for the FG game build (v0.12.2+, perf switch sample_profile).

  python -B tools/profile_report.py --run <folder> [--pid N] [--mask M] [--top 25]

Reads the "profile" events of one launch (optionally only those taken with
perf mask M) and prints, for the host Present thread and for the other
threads: where samples landed (leaf function), the innermost and the
outermost function of the mod on the stack, and for the mod's outermost
functions which module the time was spent in. Counts are wall-clock samples
(waiting included) at about 1 kHz for the Present thread.

Symbols: the mod from the linker map (<run>/mhw_fg_game_probe.map, else the
build's map when the installed DLL matches the build); system and SDK DLLs
from their exports via dbghelp (an export name is the nearest exported
symbol, so internal functions show as export+offset); the game by RVA.
"""
import argparse
import bisect
import collections
import ctypes
import ctypes.wintypes as w
import hashlib
import json
import re
from pathlib import Path

from fg_game import log_path, rows_of

REPO = Path(__file__).resolve().parents[1]
TABLES = ('leaf', 'inner', 'outer', 'outer_leaf_module')
UNKNOWN = 0xFFF


class MapSymbols:
    """Function starts from an MSVC linker map (publics and static symbols)."""

    def __init__(self, path):
        text = Path(path).read_text(encoding='utf-8', errors='replace')
        base = int(re.search(r'Preferred load address is ([0-9a-fA-F]+)', text).group(1), 16)
        found = {}
        for m in re.finditer(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s+f\s', text, re.M):
            rva = int(m.group(2), 16) - base
            if rva > 0:
                found.setdefault(rva, m.group(1))
        self.starts = sorted(found)
        self.names = [found[r] for r in self.starts]

    def name(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i < 0:
            return None, rva
        return undecorate(self.names[i]), self.starts[i]


_dbghelp = None


def dbghelp():
    global _dbghelp
    if _dbghelp is None:
        d = ctypes.WinDLL('dbghelp.dll')
        d.SymInitializeW.argtypes = [w.HANDLE, w.LPCWSTR, w.BOOL]
        d.SymLoadModuleExW.argtypes = [w.HANDLE, w.HANDLE, w.LPCWSTR, w.LPCWSTR, ctypes.c_uint64, w.DWORD, ctypes.c_void_p, w.DWORD]
        d.SymLoadModuleExW.restype = ctypes.c_uint64
        d.SymFromAddrW.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.c_void_p]
        d.UnDecorateSymbolName.argtypes = [ctypes.c_char_p, ctypes.c_char_p, w.DWORD, w.DWORD]
        d.SymSetOptions(0x2 | 0x4)  # UNDNAME | DEFERRED_LOADS
        d.SymInitializeW(w.HANDLE(0x4D48), None, False)
        _dbghelp = d
    return _dbghelp


def undecorate(name):
    if not name.startswith('?'):
        return name
    out = ctypes.create_string_buffer(1024)
    if dbghelp().UnDecorateSymbolName(name.encode('ascii', 'replace'), out, 1024, 0x1000):  # UNDNAME_NAME_ONLY
        return out.value.decode('ascii', 'replace')
    return name


class ExportSymbols:
    """Nearest exported symbol of a DLL, through dbghelp."""

    _next_base = 0x10000000

    def __init__(self, path):
        self.base = ExportSymbols._next_base
        ExportSymbols._next_base += 0x10000000
        self.ok = bool(dbghelp().SymLoadModuleExW(w.HANDLE(0x4D48), None, str(path), None, self.base, 0, None, 0))

    def name(self, rva):
        if not self.ok:
            return None, rva
        class SymbolInfo(ctypes.Structure):
            _fields_ = [('SizeOfStruct', w.ULONG), ('TypeIndex', w.ULONG), ('Reserved', ctypes.c_uint64 * 2), ('Index', w.ULONG),
                        ('Size', w.ULONG), ('ModBase', ctypes.c_uint64), ('Flags', w.ULONG), ('Value', ctypes.c_uint64),
                        ('Address', ctypes.c_uint64), ('Register', w.ULONG), ('Scope', w.ULONG), ('Tag', w.ULONG),
                        ('NameLen', w.ULONG), ('MaxNameLen', w.ULONG), ('Name', ctypes.c_wchar * 512)]
        info = SymbolInfo()
        info.SizeOfStruct = ctypes.sizeof(SymbolInfo) - 511 * ctypes.sizeof(ctypes.c_wchar)
        info.MaxNameLen = 512
        displacement = ctypes.c_uint64()
        if not dbghelp().SymFromAddrW(w.HANDLE(0x4D48), self.base + rva, ctypes.byref(displacement), ctypes.byref(info)):
            return None, rva
        return info.Name, rva - displacement.value


def module_file(name, run):
    candidates = [Path(r'C:\Windows\System32') / name]
    runtime = (run / 'sdk-runtime-dir.txt')
    if runtime.exists():
        candidates.append(Path(runtime.read_text(encoding='utf-8').strip()) / name)
    candidates += sorted(Path(r'C:\Windows\System32\DriverStore\FileRepository').glob(f'nv*/{name}'))
    return next((c for c in candidates if c.exists()), None)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def mod_symbols(run, header):
    local = run / 'mhw_fg_game_probe.map'
    if local.exists():
        return MapSymbols(local), str(local)
    build = REPO / 'build/bin/Release'
    if (build / 'mhw_fg_game_probe.map').exists() and sha(build / 'mhw_fg_game_probe.dll') == header.get('dll_sha256'):
        return MapSymbols(build / 'mhw_fg_game_probe.map'), 'build (sha matches)'
    return None, 'no matching map: RVAs only'


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--pid', type=int)
    p.add_argument('--mask', type=int, help='only events taken with this perf mask')
    p.add_argument('--top', type=int, default=25)
    a = p.parse_args()
    rows, _ = rows_of(log_path(a.run, a.pid))
    header = rows[0]
    events = [r for r in rows if r.get('event') == 'profile' and (a.mask is None or r['perf'] == a.mask)]
    if not events:
        raise SystemExit('no profile events')
    mod, mod_source = mod_symbols(a.run, header)
    resolvers = {}

    def resolver(name):
        if name not in resolvers:
            f = module_file(name, a.run) if name.lower().endswith('.dll') else None
            resolvers[name] = ExportSymbols(f) if f else None
        return resolvers[name]

    counts = collections.Counter()
    samples = [0, 0]
    for e in events:
        names = e['modules']
        samples[0] += e['samples'][0]
        samples[1] += e['samples'][1]
        for table, kind, module, rva, extra, n in e['cells']:
            mname = names[module] if module != UNKNOWN and module < len(names) else None
            xname = names[extra] if table == 3 and extra != UNKNOWN and extra < len(names) else None
            own = module == e['self']
            if mname is None:
                label = '(not in the mod)' if table in (1, 2, 3) else '(unknown module)'
            elif own and mod:
                sym, _ = mod.name(rva)
                label = f'mod!{sym}' if sym else f'mod+{rva:#x}'
            elif not own and resolver(mname):
                sym, start = resolver(mname).name(rva)
                label = f'{mname}!{sym}' if sym else f'{mname}+{rva:#x}'
            else:
                label = f'{mname}+{rva:#x}'
            if table == 3:
                label = f'{label}  ->  {xname or "(unknown module)"}'
            counts[(table, kind, label)] += n
    print(f"events {len(events)}  masks {sorted({e['perf'] for e in events})}  ms {sum(e['ms'] for e in events)}  "
          f"samples present {samples[0]} other {samples[1]}  failed {sum(e['failed'] for e in events)}  "
          f"truncated {sum(e['truncated'] for e in events)}  overflow {sum(e['overflow'] for e in events)}")
    print(f'mod symbols: {mod_source}')
    for kind, title in ((0, 'HOST PRESENT THREAD'), (1, 'OTHER THREADS (rotation)')):
        total = samples[kind] or 1
        for table in range(4):
            items = sorted(((n, label) for (t, k, label), n in counts.items() if t == table and k == kind), reverse=True)[:a.top]
            print(f'\n== {title}: {TABLES[table]}')
            for n, label in items:
                print(f'{100 * n / total:6.1f}%  {n:7d}  {label}')


if __name__ == '__main__':
    main()

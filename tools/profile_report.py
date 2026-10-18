"""Stack-sampler report for the FG game build (v0.12.2+, perf switch sample_profile).

  python -B tools/profile_report.py --run <folder> [--pid N] [--mask M] [--phase P] [--top 25]

Reads the "profile" events of one launch (optionally only those taken with
perf mask M or in one phase) and prints, for the host Present thread and for
the other threads: where samples landed (leaf function); the innermost run of
mod frames, by its first frame (inner) and the hook the game called (outer,
v0.12.3+; before that the outermost mod frame), and that hook with the module
the time was spent in; and the innermost game frame (v0.12.3+). Counts are wall-clock samples
(waiting included) at about 1 kHz for the Present thread.

Symbols: the mod from the linker map (<run>/mhw_dlss.map, mhw_dlss_alpha.map
up to 0.28.6 or the research build's mhw_fg_game_probe.map, else the build's
map when the installed DLL matches the build); system and SDK DLLs
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
TABLES = ('leaf', 'inner', 'outer', 'outer_leaf_module', 'game')
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
    """Nearest named export at or below an RVA, from the DLL's export table."""

    def __init__(self, path):
        data = Path(path).read_bytes()
        u16 = lambda o: int.from_bytes(data[o:o + 2], 'little')
        u32 = lambda o: int.from_bytes(data[o:o + 4], 'little')
        pe = u32(0x3c)
        sections = [(u32(s + 12), u32(s + 8), u32(s + 20)) for s in
                    (pe + 24 + u16(pe + 20) + 40 * i for i in range(u16(pe + 6)))]  # (rva, size, file offset)

        def offset(rva):
            for va, size, raw in sections:
                if va <= rva < va + max(size, 1):
                    return raw + rva - va
            raise ValueError(rva)
        export_rva = u32(pe + 24 + 112)  # PE32+ data directory 0
        found = {}
        if export_rva:
            e = offset(export_rva)
            functions, names, ordinals = offset(u32(e + 28)), offset(u32(e + 32)), offset(u32(e + 36))
            for i in range(u32(e + 24)):
                name_at = offset(u32(names + 4 * i))
                name = data[name_at:data.index(bytes(1), name_at)].decode('ascii', 'replace')
                found.setdefault(u32(functions + 4 * u16(ordinals + 2 * i)), name)
        self.starts = sorted(found)
        self.names = [found[r] for r in self.starts]
        self.ok = bool(self.starts)

    def name(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i < 0:
            return None, rva
        return self.names[i] + (f'+{rva - self.starts[i]:#x}' if rva != self.starts[i] else ''), self.starts[i]


def module_file(name, run):
    candidates = [Path(r'C:\Windows\System32') / name]
    runtime = (run / 'sdk-runtime-dir.txt')
    if runtime.exists():
        candidates.append(Path(runtime.read_text(encoding='utf-8').strip()) / name)
    candidates += sorted(Path(r'C:\Windows\System32\DriverStore\FileRepository').glob(f'nv*/{name}'))
    return next((c for c in candidates if c.exists()), None)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


map_names = ('mhw_dlss', 'mhw_dlss_alpha', 'mhw_fg_game_probe')


def mod_symbols(run, header):
    # The mod (mhw_dlss; mhw_dlss_alpha up to 0.28.6) or the research build
    # (mhw_fg_game_probe).
    for name in map_names:
        local = run / f'{name}.map'
        if local.exists():
            return MapSymbols(local), str(local)
    build = REPO / 'build/bin/Release'
    for name in map_names:
        if (build / f'{name}.map').exists() and sha(build / f'{name}.dll') == header.get('dll_sha256'):
            return MapSymbols(build / f'{name}.map'), 'build (sha matches)'
    return None, 'no matching map: RVAs only'


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--pid', type=int)
    p.add_argument('--mask', type=int, help='only events taken with this perf mask')
    p.add_argument('--phase', help='unarmed, armed or fg_off')
    p.add_argument('--last', type=int, help='only the last N matching events (5-second windows)')
    p.add_argument('--skip', type=int, default=0, help='drop the first N matching events (applied before --first)')
    p.add_argument('--first', type=int, help='only the first N matching events (after --skip)')
    p.add_argument('--top', type=int, default=25)
    a = p.parse_args()
    rows, _ = rows_of(log_path(a.run, a.pid))
    header = rows[0]
    events, phase = [], None
    for r in rows:  # a profile event follows its window's frame_stats line
        if r.get('event') == 'frame_stats':
            phase = r['phase']
        elif r.get('event') == 'profile' and (a.mask is None or r['perf'] == a.mask) and (a.phase is None or phase == a.phase):
            events.append(r)
    events = events[a.skip:]
    if a.first:
        events = events[:a.first]
    if a.last:
        events = events[-a.last:]
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
                label = {1: '(not in the mod)', 2: '(not in the mod)', 3: '(not in the mod)', 4: '(no game frame)'}.get(table, '(unknown module)')
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
        for table in range(len(TABLES)):
            items = sorted(((n, label) for (t, k, label), n in counts.items() if t == table and k == kind), reverse=True)[:a.top]
            print(f'\n== {title}: {TABLES[table]}')
            for n, label in items:
                print(f'{100 * n / total:6.1f}%  {n:7d}  {label}')


if __name__ == '__main__':
    main()

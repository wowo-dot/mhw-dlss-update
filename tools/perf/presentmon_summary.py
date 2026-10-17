"""Summarise PresentMon CSV captures of Monster Hunter: World.

Usage:
    py -3.12 -B tools/perf/presentmon_summary.py <folder or CSV files>
        [--slices SECONDS] [--log mhw_dlss.log] [--settle SECONDS]
        [--window HH:MM:SS HH:MM:SS] [--json out.json]

presentmon_capture.ps1 records continuously, one CSV per game launch, so a
capture holds several settings in turn. Ways to cut it:
- --slices: consecutive slices of that many seconds (steady stretches show as
  rows with similar numbers);
- --log: segments between the mod's menu changes, read from mhw_dlss.log
  ("HH:MM:SS.mmm INFO menu: dlss_mode = quality (applied now)"), skipping
  --settle seconds after each change (default 5);
- --window: one wall-clock interval.

Each row shows:
- duration and present count;
- presented and displayed frame rates;
- "1% low" and "0.1% low": 1000 / the 99th and 99.9th percentile display
  interval;
- p50, p99 and maximum display interval;
- GPU and CPU busy per present;
- PC latency when Reflex reports it;
- dropped frames.

Column names differ between versions (2.4: MsBetweenPresents,
MsBetweenDisplayChange, MsGPUBusy...; 2.6: FrameTime, DisplayedTime,
GPUBusy...); both are read, and columns a version does not write are skipped.

PresentMon 2.6.0 writes --date_time hours off from local time (2026-10-13:
three hours ahead on a UTC+3 machine). Each capture's clock is corrected by
the whole quarter hours between its last row and the file's modification
time, so the times match the mod's menu log. With frame
generation every present (rendered or generated) is counted; the mod's own
frame_stats give the rendered rate.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import re
import sys
from collections import Counter
from pathlib import Path

GAME = 'monsterhunterworld.exe'
DATE_TIME = re.compile(r'(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)(?:\.(\d+))?')
# Live menu changes, and the perf_ab diagnostic's "perf: dlss = paused" lines.
MENU = re.compile(r'^(\d\d):(\d\d):(\d\d)\.(\d{3}) INFO (?:menu|perf): (\w+) = (\S+) \(applied now\)')


def number(value: str | None) -> float | None:
    if value is None:
        return None
    value = value.strip()
    if not value or value.upper() == 'NA':
        return None
    try:
        result = float(value)
    except ValueError:
        return None
    return result if math.isfinite(result) else None


def timestamp(value: str | None) -> dt.datetime | None:
    """PresentMon's --date_time value: local time with nanoseconds."""
    m = DATE_TIME.fullmatch((value or '').strip())
    if not m:
        return None
    year, month, day, hour, minute, second = (int(x) for x in m.groups()[:6])
    micro = int((m.group(7) or '0').ljust(9, '0')[:6])
    return dt.datetime(year, month, day, hour, minute, second, micro)


def percentile(values: list[float], p: float) -> float | None:
    """Nearest-rank percentile (p in 0..100) of the values."""
    if not values:
        return None
    ordered = sorted(values)
    rank = max(1, math.ceil(p / 100 * len(ordered)))
    return ordered[rank - 1]


def mean(values: list[float]) -> float | None:
    return sum(values) / len(values) if values else None


def column(rows: list[dict], *names: str) -> list[float]:
    """The first of `names` present in the rows, as numbers (NA skipped)."""
    if not rows:
        return []
    for name in names:
        if name in rows[0]:
            return [v for v in (number(r.get(name)) for r in rows) if v is not None]
    return []


def frame_rate(intervals_ms: list[float]) -> float | None:
    total = sum(intervals_ms)
    return len(intervals_ms) * 1000 / total if total > 0 else None


def low(intervals_ms: list[float], p: float) -> float | None:
    worst = percentile(intervals_ms, p)
    return 1000 / worst if worst else None


def times(intervals_ms: list[float]) -> dict:
    return {
        'p50': percentile(intervals_ms, 50), 'p95': percentile(intervals_ms, 95),
        'p99': percentile(intervals_ms, 99), 'p99_9': percentile(intervals_ms, 99.9),
        'max': max(intervals_ms) if intervals_ms else None,
    }


def clock_offset(path: Path, last: dt.datetime) -> dt.timedelta:
    """Whole quarter hours between the capture's last row and the file's
    modification time (PresentMon closes the file after its last row)."""
    written = dt.datetime.fromtimestamp(path.stat().st_mtime)
    quarters = round((last - written) / dt.timedelta(minutes=15))
    return dt.timedelta(minutes=15 * quarters)


def load(path: Path) -> list[dict]:
    """The game's presents on its main swap chain, with `_t` (local datetime
    or None)."""
    with path.open(newline='', encoding='utf-8-sig') as f:
        rows = [r for r in csv.DictReader(f) if (r.get('Application') or '').lower() == GAME]
    if not rows:
        return []
    # The game's swap chains, one after another: frame generation switched live
    # (v0.27.4) rebuilds the chain, so a launch has several. A chain that
    # presents at the same time as a bigger one (an overlay's) is left out:
    # chains are taken by present count while each lies before or after every
    # chain taken before (half a second of overlap allowed for a switch); one
    # inside another's span is concurrent.
    for r in rows:
        r['_t'] = timestamp(r.get('CPUStartDateTime'))
    chains = Counter(r.get('SwapChainAddress', '') for r in rows)
    spans: dict[str, list] = {}
    for r in rows:
        if r['_t']:
            span = spans.setdefault(r.get('SwapChainAddress', ''), [r['_t'], r['_t']])
            span[0] = min(span[0], r['_t'])
            span[1] = max(span[1], r['_t'])
    slack = dt.timedelta(seconds=0.5)
    taken: list[str] = []
    for chain, _ in chains.most_common():
        span = spans.get(chain)
        if span is None:
            if not taken:
                taken.append(chain)
            continue
        def sequential(other):
            before = span[0] < other[0] and span[1] <= other[0] + slack
            after = span[1] > other[1] and span[0] >= other[1] - slack
            return before or after
        if all(sequential(spans[t]) for t in taken if t in spans):
            taken.append(chain)
    kept = set(taken)
    rows = [r for r in rows if r.get('SwapChainAddress', '') in kept]
    for r in rows:
        r['_chains'] = len(chains)  # seen, the overlays' included
        r['_chains_used'] = len(kept)
    stamped = [r['_t'] for r in rows if r['_t']]
    if stamped:
        offset = clock_offset(path, stamped[-1])
        if offset:
            for r in rows:
                if r['_t']:
                    r['_t'] -= offset
    return rows


def stats(label: str, rows: list[dict]) -> dict:
    result: dict = {'label': label, 'presents': len(rows)}
    if not rows:
        return result
    presents = column(rows, 'MsBetweenPresents', 'FrameTime')
    displayed = [v for v in column(rows, 'MsBetweenDisplayChange', 'DisplayedTime') if v > 0]
    result.update(
        swap_chains=rows[0].get('_chains'),
        swap_chains_used=rows[0].get('_chains_used'),
        duration_s=sum(presents) / 1000 if presents else None,
        presented_fps=frame_rate(presents),
        displayed_fps=frame_rate(displayed),
        present_ms=times(presents),
        display_ms=times(displayed),
        low_1pct_fps=low(displayed or presents, 99),
        low_0_1pct_fps=low(displayed or presents, 99.9),
        cpu_busy_ms=mean(column(rows, 'MsCPUBusy', 'CPUBusy')),
        gpu_busy_ms=mean(column(rows, 'MsGPUBusy', 'GPUBusy', 'MsGPUActive')),
        gpu_time_ms=mean(column(rows, 'MsGPUTime', 'GPUTime')),
        display_latency_ms=mean(column(rows, 'MsUntilDisplayed', 'DisplayLatency')),
        pc_latency_ms=mean(column(rows, 'MsPCLatency', 'PCLatency')),
        input_to_photon_ms=mean(column(rows, 'MsAllInputToPhotonLatency', 'AllInputToPhotonLatency')),
    )
    shown = next((n for n in ('MsUntilDisplayed', 'DisplayLatency') if n in rows[0]), None)
    if shown:
        result['dropped'] = sum(1 for r in rows if number(r.get(shown)) is None)
    elif 'Dropped' in rows[0]:
        result['dropped'] = sum(1 for r in rows if number(r.get('Dropped')) == 1)
    if 'FrameType' in rows[0]:
        result['frame_types'] = dict(Counter(r.get('FrameType') or 'NA' for r in rows))
    stamped = [r['_t'] for r in rows if r.get('_t')]
    if stamped:
        result['first'] = stamped[0].isoformat(sep=' ', timespec='milliseconds')
        result['last'] = stamped[-1].isoformat(sep=' ', timespec='milliseconds')
    return result


def between(rows: list[dict], start: dt.datetime | None, end: dt.datetime | None) -> list[dict]:
    return [r for r in rows if r.get('_t') and (start is None or r['_t'] >= start) and (end is None or r['_t'] < end)]


def slices(name: str, rows: list[dict], seconds: float) -> list[dict]:
    stamped = [r for r in rows if r.get('_t')]
    if not stamped:
        return []
    out, start, step = [], stamped[0]['_t'], dt.timedelta(seconds=seconds)
    while start <= stamped[-1]['_t']:
        part = between(stamped, start, start + step)
        if part:
            out.append(stats(f'{name} {start:%H:%M:%S}', part))
        start += step
    return out


def menu_changes(log: Path, day: dt.date) -> list[tuple[dt.datetime, str]]:
    """(time, 'key=value') for each menu change applied live, in order."""
    changes = []
    for line in log.read_text(encoding='utf-8', errors='replace').splitlines():
        m = MENU.match(line)
        if m:
            h, mi, s, ms = (int(x) for x in m.groups()[:4])
            changes.append((dt.datetime.combine(day, dt.time(h, mi, s, ms * 1000)), f'{m.group(5)}={m.group(6)}'))
    return changes


def segments(name: str, rows: list[dict], log: Path, settle: float) -> list[dict]:
    """One row per stretch between menu changes, after `settle` seconds."""
    stamped = [r for r in rows if r.get('_t')]
    if not stamped:
        return []
    first, last = stamped[0]['_t'], stamped[-1]['_t']
    changes = [c for c in menu_changes(log, first.date()) if first <= c[0] <= last]
    marks = [(first, 'start')] + changes + [(last + dt.timedelta(microseconds=1), 'end')]
    out = []
    for (start, label), (end, _) in zip(marks, marks[1:]):
        part = between(stamped, start + dt.timedelta(seconds=settle), end)
        if part:
            out.append(stats(f'{name} {start:%H:%M:%S} {label}', part))
    return out


def captures(arguments: list[str]) -> list[Path]:
    paths: list[Path] = []
    for argument in arguments:
        p = Path(argument)
        paths.extend(sorted(p.glob('*.csv')) if p.is_dir() else [p])
    return paths


def fmt(value, digits=1) -> str:
    return '-' if value is None else f'{value:.{digits}f}'


def table(results: list[dict]) -> str:
    header = ('capture', 'seconds', 'presents', 'fps', 'shown', '1% low', 'p50 ms', 'p99 ms', 'max ms',
              'GPU busy', 'CPU busy', 'disp lat', 'PC lat', 'dropped')
    lines = [header]
    for r in results:
        d = r.get('display_ms') or {}
        lines.append((r['label'], fmt(r.get('duration_s')), str(r.get('presents', 0)), fmt(r.get('presented_fps')),
                      fmt(r.get('displayed_fps')), fmt(r.get('low_1pct_fps')), fmt(d.get('p50'), 2), fmt(d.get('p99'), 2),
                      fmt(d.get('max'), 2), fmt(r.get('gpu_busy_ms'), 2), fmt(r.get('cpu_busy_ms'), 2),
                      fmt(r.get('display_latency_ms'), 2), fmt(r.get('pc_latency_ms'), 2), str(r.get('dropped', '-'))))
    widths = [max(len(line[i]) for line in lines) for i in range(len(header))]
    return '\n'.join('  '.join(cell.ljust(widths[i]) for i, cell in enumerate(line)) for line in lines)


def clock(value: str, day: dt.date) -> dt.datetime:
    return dt.datetime.combine(day, dt.time.fromisoformat(value))


def summarise(path: Path, slice_s: float | None = None, log: Path | None = None, settle: float = 5,
              window: tuple[str, str] | None = None) -> list[dict]:
    rows = load(path)
    name = path.stem
    if window:
        stamped = [r for r in rows if r.get('_t')]
        day = stamped[0]['_t'].date() if stamped else dt.date.today()
        rows = between(rows, clock(window[0], day), clock(window[1], day))
    results = [stats(name, rows)]
    if slice_s:
        results += slices(name, rows, slice_s)
    if log:
        results += segments(name, rows, log, settle)
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('paths', nargs='+', help='capture folders or CSV files')
    parser.add_argument('--slices', type=float, help='also summarise consecutive slices of this many seconds')
    parser.add_argument('--log', type=Path, help="also summarise the stretches between the mod's menu changes")
    parser.add_argument('--settle', type=float, default=5, help='seconds skipped after each menu change (default 5)')
    parser.add_argument('--window', nargs=2, metavar=('START', 'END'), help='only this wall-clock interval (HH:MM:SS)')
    parser.add_argument('--json', type=Path, help='also write the summaries as JSON')
    args = parser.parse_args(argv)
    paths = captures(args.paths)
    if not paths:
        print('no CSV captures found', file=sys.stderr)
        return 1
    results = [r for p in paths for r in summarise(p, args.slices, args.log, args.settle, args.window)]
    print(table(results))
    if args.json:
        args.json.write_text(json.dumps(results, indent=1), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

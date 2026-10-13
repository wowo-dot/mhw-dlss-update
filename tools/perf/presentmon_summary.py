"""Summarise PresentMon CSV captures of Monster Hunter: World.

Usage:
    py -3.12 -B tools/perf/presentmon_summary.py <folder or CSV files> [--json out.json]

For each capture (presentmon_capture.ps1 writes them under local/perf/<session>/):
- duration and present count;
- presented and displayed frame rates;
- frame-time percentiles: presents and display changes;
- "1% low" and "0.1% low": 1000 / the 99th and 99.9th percentile frame time;
- CPU and GPU busy per present;
- PC latency when Reflex reports it, and input-to-photon latency;
- dropped frames;
- frame types when PresentMon reports them.

Columns a PresentMon version does not write are skipped. With frame
generation every present (rendered or generated) is counted; the mod's own
frame_stats give the rendered rate.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from collections import Counter
from pathlib import Path

GAME = 'monsterhunterworld.exe'


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
    """The first of `names` present in the capture, as numbers (NA skipped)."""
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


def summarise(path: Path) -> dict:
    with path.open(newline='', encoding='utf-8-sig') as f:
        rows = [r for r in csv.DictReader(f) if (r.get('Application') or '').lower() == GAME]
    result: dict = {'file': path.name, 'presents': len(rows)}
    if not rows:
        return result
    # The swap chain with the most presents (the game's; overlays add others).
    chains = Counter(r.get('SwapChainAddress', '') for r in rows)
    chain = chains.most_common(1)[0][0]
    rows = [r for r in rows if r.get('SwapChainAddress', '') == chain]
    presents = column(rows, 'MsBetweenPresents')
    displayed = [v for v in column(rows, 'MsBetweenDisplayChange') if v > 0]
    result.update(
        presents=len(rows),
        swap_chains=len(chains),
        duration_s=sum(presents) / 1000 if presents else None,
        presented_fps=frame_rate(presents),
        displayed_fps=frame_rate(displayed),
        present_ms=times(presents),
        display_ms=times(displayed),
        low_1pct_fps=low(displayed or presents, 99),
        low_0_1pct_fps=low(displayed or presents, 99.9),
        cpu_busy_ms=mean(column(rows, 'MsCPUBusy')),
        gpu_busy_ms=mean(column(rows, 'MsGPUBusy', 'MsGPUActive')),
        gpu_time_ms=mean(column(rows, 'MsGPUTime')),
        pc_latency_ms=mean(column(rows, 'MsPCLatency')),
        input_to_photon_ms=mean(column(rows, 'MsAllInputToPhotonLatency')),
    )
    if 'MsUntilDisplayed' in rows[0]:
        result['dropped'] = sum(1 for r in rows if number(r.get('MsUntilDisplayed')) is None)
    elif 'Dropped' in rows[0]:
        result['dropped'] = sum(1 for r in rows if number(r.get('Dropped')) == 1)
    if 'FrameType' in rows[0]:
        result['frame_types'] = dict(Counter(r.get('FrameType') or 'NA' for r in rows))
    for name in ('CPUStartTime', 'CPUStartDateTime', 'TimeInSeconds', 'CPUStartQPC'):
        if name in rows[0]:
            result['first'] = rows[0][name]
            result['last'] = rows[-1][name]
            break
    return result


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
              'GPU busy', 'CPU busy', 'PC lat', 'dropped')
    lines = [header]
    for r in results:
        d = r.get('display_ms') or {}
        lines.append((r['file'], fmt(r.get('duration_s')), str(r.get('presents', 0)), fmt(r.get('presented_fps')),
                      fmt(r.get('displayed_fps')), fmt(r.get('low_1pct_fps')), fmt(d.get('p50'), 2), fmt(d.get('p99'), 2),
                      fmt(d.get('max'), 2), fmt(r.get('gpu_busy_ms'), 2), fmt(r.get('cpu_busy_ms'), 2),
                      fmt(r.get('pc_latency_ms'), 2), str(r.get('dropped', '-'))))
    widths = [max(len(line[i]) for line in lines) for i in range(len(header))]
    return '\n'.join('  '.join(cell.ljust(widths[i]) for i, cell in enumerate(line)) for line in lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('paths', nargs='+', help='capture folders or CSV files')
    parser.add_argument('--json', type=Path, help='also write the summaries as JSON')
    args = parser.parse_args(argv)
    paths = captures(args.paths)
    if not paths:
        print('no CSV captures found', file=sys.stderr)
        return 1
    results = [summarise(p) for p in paths]
    print(table(results))
    if args.json:
        args.json.write_text(json.dumps(results, indent=1), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

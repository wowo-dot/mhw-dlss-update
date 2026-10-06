"""Review both SDK channels and their shared lifetime; generation-checked controls."""
import argparse
import hashlib
import json
from pathlib import Path
from reflex_frames import validate as reflex_review, control, HOST_SHA, check
from review_continuous_dlaa import review as temporal_review


def identity(rows):
    check(rows and rows[0].get('event') == 'start', 'combined start')
    h = rows[0]
    check(h.get('schema') == 3 and h.get('version') in ('0.5.0', '0.5.1') and
          h.get('experiment') == 'reflex-temporal-coexistence', 'combined identity')
    check(h.get('host_sha256') == HOST_SHA and h.get('mode') == 0 and
          h.get('target_mode') == 1 and h.get('upscaling') is True and
          h.get('temporal_version') in ('1.21.5', '1.25.44'), 'combined scope')
    return h


def parse(path):
    check(path.stat().st_size <= 8 * 1024 * 1024, 'bounded combined log')
    rows = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
    identity(rows)
    return rows


def cross_links(rows, inputs, startup, expected_dll):
    """Check the join only. Acceptance also requires both existing reviewers."""
    h = identity(rows)
    check(h.get('dll_sha256') == expected_dll.lower(), 'combined DLL identity')
    for channel in (inputs, startup):
        check(channel and channel[0].get('event') == 'start', 'paired start')
        first = channel[0]
        check(all(first.get(k) == h.get(k) for k in ('pid', 'creation', 'dll_sha256', 'host_sha256')),
              'same combined process and DLL')
        check(first.get('version') == h['temporal_version'], 'preserved temporal version')
    check(inputs[0].get('base') == startup[0].get('base'), 'same temporal image')

    def one(event):
        found = [r for r in rows if r.get('event') == event]
        check(len(found) == 1, event)
        return found[0]

    links = [r for r in rows if r.get('event') == 'temporal_coexistence']
    check([r.get('phase') for r in links] == ['prepared', 'armed', 'drained'], 'combined lifecycle receipts')
    checked = one('device_checked')
    device = checked.get('device')
    graphics = [r for r in startup if r.get('event') == 'graphics']
    check(len(graphics) == 1 and graphics[0].get('device') == device and
          type(device) is int and device > 0, 'NGX and Reflex native device')
    for status in inputs:
        if status.get('event') == 'continuous' and status.get('frames', 0) > 0:
            check(isinstance(status.get('sdk'), dict) and status['sdk'].get('device') == device,
                  'evaluated temporal native device')
    for n, link in enumerate(links):
        check(link.get('version') == h['temporal_version'] and link.get('device') == device and
              link.get('input_log') == f"inputs-{h['pid']}-{h['creation']}.jsonl" and
              link.get('startup_log') == f"startup-{h['pid']}-{h['creation']}.jsonl" and
              link.get('prepared') is True and link.get('NGX_shutdown_deferred') is True,
              'linked temporal ownership')
        check(link.get('installed') is (n > 0) and link.get('finished') is (n == 2) and
              link.get('quiescent') is (n == 2), 'temporal cleanup completion')
    check(one('finished').get('temporal_drained') is True, 'final combined cleanup')
    order = [checked, links[0], one('frames_ready'), links[1], one('frame_hooks_installed'),
             one('frames_armed'), links[2], one('frame_finished'), one('presentation_cleanup'),
             one('sdk_shutdown'), one('finished')]
    check(all(rows.index(a) < rows.index(b) for a, b in zip(order, order[1:])),
          'temporal cleanup before Reflex drain and SDK shutdown')
    if h['version'] == '0.5.1':
        anchor = one('frame_anchor_check')
        check(anchor.get('policy') == 'bounded-worker-stamp-v1' and anchor.get('passed') is True and
              anchor.get('reason') == '' and anchor.get('hooks_created') is False and
              anchor.get('device') == device, 'qualified frame anchors')
        check(type(anchor.get('attempts')) is int and 1 <= anchor['attempts'] <= 64 and
              anchor.get('attempt_limit') == 64 and anchor.get('timeout_ms') == 100 and
              type(anchor.get('elapsed_ms')) is int and 0 <= anchor['elapsed_ms'] <= 150,
              'bounded anchor sampling')
        check(anchor.get('first_status') == (0 if anchor['attempts'] == 1 else 1) and
              anchor.get('final_status') == 0, 'retry moving counters only')
        first = anchor.get('first', [])
        check(len(first) == 5 and all(type(v) is int and v >= 0 for v in first) and
              first[2] in (0, 1) and first[4] in (0, 1) and
              (first[0] == first[3] and first[2] == first[4]) is (anchor['attempts'] == 1),
              'first stamp agrees with retry reason')
        last = anchor.get('last', [])
        check(len(last) == 5 and all(type(v) is int and v >= 0 for v in last) and
              last[2] in (0, 1) and last[0] == last[3] and last[2] == last[4], 'coherent final stamp')
        installed = one('frame_hooks_installed')
        check(all(anchor.get(k) == installed.get(k) for k in ('engine', 'renderer', 'backend')) and
              rows.index(links[1]) < rows.index(anchor) < rows.index(installed), 'anchors before frame hooks')


def review(rows, inputs, startup, expected_dll):
    cross_links(rows, inputs, startup, expected_dll)
    normalized = [dict(r) for r in rows]
    # Only after checking the new wrapper contract, reuse the unchanged
    # accepted v0.4.2 Reflex rules. Real sleep/markers/On->Off remain mandatory.
    normalized[0].update(schema=2, version='0.4.2', experiment='reflex-live-frames', upscaling=False)
    reflex = reflex_review(normalized)
    temporal = temporal_review(inputs, startup, expected_dll)
    check(reflex['live_frames_passed'] and temporal['accepted'] and temporal['frames'] > 0,
          'both SDK channels completed')
    return dict(schema=1, accepted=True, version=rows[0]['version'], pid=rows[0]['pid'], creation=rows[0]['creation'],
                same_native_device=True, temporal_drained_before_Reflex_shutdown=True,
                NGX_shutdown_deferred=True, reflex=reflex, temporal=temporal,
                latency_improvement_measured=False, visual_quality_verified=False)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('arm', 'stop', 'review'))
    p.add_argument('--log', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--dll-sha256')
    a = p.parse_args()
    check(not a.output.exists(), 'preserve combined receipt')
    rows = parse(a.log)
    if a.action == 'review':
        check(bool(a.dll_sha256), 'expected DLL digest')
        h = rows[0]
        def channel(prefix):
            path = a.log.parent / f"{prefix}-{h['pid']}-{h['creation']}.jsonl"
            check(path.stat().st_size <= 64 * 1024 * 1024, 'bounded paired log')
            return [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines()]
        result = review(rows, channel('inputs'), channel('startup'), a.dll_sha256)
    else:
        result = control(rows, a.log, a.action)
    result['log_sha256'] = hashlib.sha256(a.log.read_bytes()).hexdigest()
    with a.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

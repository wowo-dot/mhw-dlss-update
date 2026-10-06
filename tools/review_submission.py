"""Offline review of versioned diagnostic journals; never authorizes rendering.

Reconstruct command-list recordings before comparing declarations. CPU callback
order across different lists is not GPU order. Overflow stays a coverage blocker,
even when a useful submission/signal/completed-value chain survived in the file.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import struct


class ReviewError(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise ReviewError(reason)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object)


def fnv32(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def decode_journal(data):
    require(len(data) in (753832, 753944), "journal size")
    magic, schema, producer, size, pid, creation = struct.unpack_from("<QIIIIQ", data)
    require(magic == 0x314C4E4A4457484D and size == len(data) and
            (schema, producer, size) in ((1, 801, 753832), (2, 1100, 753944), (2, 1200, 753944), (2, 1300, 753944), (2, 1301, 753944), (2, 1400, 753944), (2, 1500, 753944)), "journal identity")
    state, flags, ready, unused, deadline, command, tick, order, reserved, prefix = struct.unpack_from("<4I4Q2I", data, 32)
    require(1 <= state <= 3 and flags <= 15 and unused == 0, "journal status")
    require(ready == 2 and command and order and 0 < reserved <= 2048 and prefix <= reserved, "checkpoint")
    result = dict(pid=pid, creation=creation, producer=producer, state=state, flags=flags, deadline=deadline,
                  checkpoint=dict(command=command, tick=tick, order=order, reserved=reserved, prefix=prefix),
                  pending=0, failed=0, calls=[])
    names = "command caller tick thread call first count kind result queue fence value entry returned".split()
    barrier_base = 88 + 2048 * 96
    list_base = barrier_base + 16384 * 32
    for index in range(2048):
        offset = 88 + index * 96
        slot, checksum = struct.unpack_from("<II", data, offset)
        if slot == 0:
            continue
        if slot in (1, 3):
            result["pending" if slot == 1 else "failed"] += 1
            continue
        require(slot == 2, "invalid slot state")
        raw = data[offset + 8:offset + 96]
        call = dict(zip(names, struct.unpack("<3Q6I5Q", raw)))
        first, count, kind = call["first"], call["count"], call["kind"]
        require(call["call"] == index + 1 and kind <= 8 and 0 < call["entry"] < call["returned"], "invalid call")
        extra = b""
        if kind == 0:
            require(count <= 1024 and first + count <= 16384, "barrier range")
            extra = data[barrier_base + first * 32:barrier_base + (first + count) * 32]
            call["barriers"] = []
            for n in range(count):
                btype, bflags = struct.unpack_from("<II", extra, n * 32)
                barrier = dict(type=btype, flags=bflags)
                if btype == 0:
                    resource, subresource, before, after = struct.unpack_from("<QIII", extra, n * 32 + 8)
                    barrier.update(resource=resource, subresource=subresource, before=before, after=after)
                elif btype == 1:
                    before, after = struct.unpack_from("<QQ", extra, n * 32 + 8)
                    barrier.update(resource_before=before, resource_after=after)
                elif btype == 2:
                    barrier["resource"] = struct.unpack_from("<Q", extra, n * 32 + 8)[0]
                else:
                    raise ReviewError("unknown barrier type")
                call["barriers"].append(barrier)
        elif kind == 4:
            require(count <= 128 and first + count <= 4096, "submission range")
            extra = data[list_base + first * 8:list_base + (first + count) * 8]
            call["lists"] = list(struct.unpack("<" + "Q" * count, extra))
        else:
            require(count == 0, "unexpected call payload")
        require(fnv32(raw + extra) == checksum, f"checksum at call {index + 1}")
        result["calls"].append(call)
    result["drained"] = False
    if schema == 2:
        terminal_ready, checksum = struct.unpack_from("<II", data, 753832)
        require(terminal_ready <= 2, "terminal state")
        if terminal_ready == 2:
            payload = data[753840:]
            require(fnv32(payload) == checksum, "terminal checksum")
            reason, terminal_flags, frontier, unused, terminal_order = struct.unpack_from("<4IQ", payload)
            require(state == 3 and 1 <= reason <= 7 and terminal_flags == flags and unused == 0 and
                    frontier == len(result["calls"]) + result["failed"] and not result["pending"] and
                    terminal_order > max((c["returned"] for c in result["calls"]), default=0), "terminal frontier")
            names = "status issue reset close submission signal completed command queue fence marker value completed_order".split()
            completion = dict(zip(names, struct.unpack_from("<7I4x6Q", payload, 24)))
            require(completion["status"] <= 2 and completion["issue"] <= 5, "terminal completion")
            result["drained"] = True
            result["termination"] = dict(reason=reason, flags=terminal_flags, frontier=frontier,
                                         order=terminal_order, **completion)
    return result


def succeeded(call):
    return call["result"] < 0x80000000


def recording(calls, command, submission):
    """Resolve the recording submitted now, not an earlier use of this pointer."""
    resets = [c for c in calls if c["command"] == command and c["kind"] == 1 and c["entry"] < submission["entry"]]
    require(resets, "recording has no observed Reset")
    reset = max(resets, key=lambda c: c["entry"])
    require(succeeded(reset) and reset["returned"] < submission["entry"], "Reset failed or overlaps submission")
    events = sorted((c for c in calls if c["command"] == command and
                     reset["entry"] <= c["entry"] < submission["returned"]), key=lambda c: c["entry"])
    require(all(c["returned"] < submission["entry"] for c in events), "recording overlaps submission")
    require(all(a["returned"] < b["entry"] for a, b in zip(events, events[1:])), "concurrent recording on one list")
    closes = [c for c in events if c["kind"] == 2]
    require(len(closes) == 1 and succeeded(closes[0]) and events[-1] is closes[0], "missing, failed or nonterminal Close")
    require(all(c["kind"] in (0, 1, 2) for c in events), "unsupported recording command/bundle")
    return dict(command=command, reset=reset["call"], close=closes[0]["call"],
                reset_order=reset["returned"], close_order=closes[0]["entry"], events=events)


def fence_chain(calls, submission):
    """Observed API chain only. Absence of unobserved writers is not established."""
    queue = submission["queue"]
    signals = sorted((c for c in calls if c["kind"] == 5 and c["queue"] == queue and
                      c["entry"] > submission["returned"]), key=lambda c: c["entry"])
    require(signals, "no same-queue signal after submission")
    signal = signals[0]
    require(succeeded(signal) and signal["fence"] and 0 < signal["value"] < (1 << 64) - 1, "invalid queue signal")
    reads = sorted((c for c in calls if c["kind"] == 7 and c["fence"] == signal["fence"] and
                    c["entry"] > signal["returned"]), key=lambda c: c["entry"])
    match = None
    previous_value = None
    for read in reads:
        require(read["value"] != (1 << 64) - 1, "device removal reported by fence")
        require(previous_value is None or read["value"] >= previous_value, "fence read rewound")
        previous_value = read["value"]
        if read["value"] >= signal["value"]:
            match = read
            break
    require(match is not None, "no completed-value observation reaches the signal")
    writers = sorted((c for c in calls if c["kind"] in (5, 8) and c["fence"] == signal["fence"] and
                      c["entry"] < match["returned"]), key=lambda c: c["entry"])
    require(all(c["kind"] == 5 and c["queue"] == queue and succeeded(c) for c in writers), "competing or failed fence writer")
    require(all(a["returned"] < b["entry"] and a["value"] < b["value"] for a, b in zip(writers, writers[1:])), "ambiguous or nonmonotonic fence writes")
    queue_calls = sorted((c for c in calls if c["kind"] in (4, 5, 6) and c["queue"] == queue and
                         c["returned"] >= submission["entry"] and c["entry"] <= signal["returned"]),
                         key=lambda c: c["entry"])
    require(all(a["returned"] < b["entry"] for a, b in zip(queue_calls, queue_calls[1:])), "overlapping queue calls")
    require(all(succeeded(c) for c in queue_calls), "queue operation failed")
    return dict(queue=queue, fence=signal["fence"], signal_call=signal["call"], signal_value=signal["value"],
                completed_call=match["call"], completed_value=match["value"], completed_order=match["returned"],
                elapsed_tick_ms=match["tick"] - submission["tick"],
                unobserved_writers_excluded=False)


def review(journal, capture, binding):
    calls = journal["calls"]
    require(not journal["pending"] and not journal["failed"], "pending or failed journal slots")
    require([c["call"] for c in calls] == list(range(1, len(calls) + 1)), "journal has a sequence hole")
    require(capture["schema"] == 1 and capture["producer"] in (801, 1100, 1200, 1300, 1301, 1400, 1500) and
            capture["producer"] == journal.get("producer", 801) and capture["evaluate_index"] == 0, "capture identity")
    require(all(capture[k] for k in ("committed", "command_verified", "boundary_clean", "handle_record_verified", "handle_binding_matched")), "unverified Evaluate inputs")
    require(capture["contract"]["complete"] and not capture["contract"]["fault"] and capture["contract"]["queries"] == 17, "parameter capture incomplete")
    require(capture["inspection"]["complete"] and not capture["inspection"]["fault"], "resource inspection incomplete")
    mark = journal["checkpoint"]
    require(binding["index"] == 0 and binding["command"] == mark["command"] and binding["thread"] == capture["thread"] and
            0 <= capture["tick"] - mark["tick"] <= 2000 and mark["reserved"] <= len(calls), "checkpoint binding")
    require(len(binding["resources"]) == 5 and all(binding["resources"][i] for i in (0, 1, 3, 4)), "missing resource")
    resources = capture["contract"]["resources"]
    require([r["key"] for r in resources] == list(range(5)), "resource key layout")
    require(all(r["value"] == binding["resources"][r["key"]] and r["attempted"] and r["completed"] and
                r["present"] and r["result"] == 1 and r["null"] == (r["value"] == 0) for r in resources), "resource binding differs from capture")
    require(capture["barriers"]["command"] == binding["command"], "command binding differs from capture")
    orders = [mark["order"]] + [c[k] for c in calls for k in ("entry", "returned")]
    require(len(set(orders)) == len(orders), "duplicate event order")
    require(all(c["entry"] < c["returned"] and c["entry"] > 0 for c in calls), "invalid event order")
    require(all(c["call"] <= mark["reserved"] for c in calls if c["entry"] < mark["order"]), "invalid reservation frontier")
    submissions = sorted((c for c in calls if c["kind"] == 4 and binding["command"] in c["lists"] and
                          c["entry"] > mark["order"]), key=lambda c: c["entry"])
    require(submissions, "no submission of the Evaluate list")
    submit = submissions[0]
    require(submit["queue"] and len(submit["lists"]) == len(set(submit["lists"])) and all(submit["lists"]), "invalid submission lists")
    selected = submit["lists"].index(binding["command"])
    recordings = [recording(calls, command, submit) for command in submit["lists"][:selected + 1]]
    target = recordings[-1]
    require(target["reset_order"] < mark["order"] < target["close_order"], "Evaluate list was reset or closed before submission")
    require(not any(c["entry"] < mark["order"] < c["returned"] for c in target["events"]), "Evaluate overlaps selected-list callback")
    by_resource = {value: key for key, value in enumerate(binding["resources"]) if value}
    require(len(by_resource) == sum(bool(v) for v in binding["resources"]), "resource pointers alias")
    states = {key: None for key in by_resource.values()}
    transitions, uavs, inversions = [], [], []
    for ri, rec in enumerate(recordings):
        for call in rec["events"]:
            if ri == selected and call["entry"] > mark["order"]:
                continue
            for barrier in call.get("barriers", []):
                require(barrier["type"] != 1, "aliasing barrier prevents state inference")
                key = by_resource.get(barrier.get("resource"))
                if key is None:
                    continue
                require(barrier["flags"] == 0, "split resource barrier")
                event = dict(key=key, call=call["call"], recording_reset=rec["reset"], list_index=ri,
                             cpu_recorded_after_evaluate=call["entry"] > mark["order"])
                if barrier["type"] == 2:
                    uavs.append(event)
                    continue  # A UAV barrier is not a state transition.
                require(barrier["type"] == 0 and barrier["subresource"] == 0xFFFFFFFF, "unsupported subresource transition")
                known = states[key]
                require(known is None or known == barrier["before"], "resource declaration discontinuity")
                states[key] = barrier["after"]
                event.update(before=barrier["before"], after=barrier["after"])
                transitions.append(event)
                if event["cpu_recorded_after_evaluate"]:
                    inversions.append(event)
    expected = {key: (state == 8 if key == 4 else state in (0x40, 0xC0)) for key, state in states.items()}
    try:
        fence = fence_chain(calls, submit)
        fence_error = None
    except ReviewError as error:
        fence, fence_error = None, str(error)
    blockers = []
    if journal["flags"]:
        blockers.append("original journal flags remain set; no full-coverage qualification")
    if not all(expected.values()):
        blockers.append("missing or unexpected resource declarations")
    if fence_error:
        blockers.append(fence_error)
    blockers.extend(["initial states, resource lifetime and all cross-queue uses are unverified",
                     "resource contents, jitter, motion convention and frame alignment are unverified"])
    packet = capture.get("frame_packet")
    if capture["producer"] == 1100 and packet is not None:
        require(packet["evaluate_bound"] and not packet["rejection"] and packet["stage_count"] == 5 and
                packet["command"] == binding["command"] and packet["producer"]["valid"] and
                packet["evaluate_frame"] == packet["producer"]["frame"] and
                packet["resources"] == [binding["resources"][i] for i in (0, 1, 4, 3)], "frame packet binding")
    projection = None
    if capture["producer"] in (1200, 1300, 1301, 1400, 1500):
        require(bool(capture.get("has_frame_packet")) == (packet is not None), "frame packet presence")
        if packet is not None:
            validate_publication_packet(packet, binding)
            if capture["producer"] in (1300, 1301, 1400, 1500):
                projection = review_projection(packet["producer"], typed=capture["producer"] in (1301, 1400, 1500))
    return dict(pid=journal["pid"], original_journal_flags=journal["flags"], completed_records=len(calls),
                journal_drained=journal.get("drained", False), termination=journal.get("termination"),
                frame_packet_bound=bool(packet and packet.get("evaluate_bound") and not packet.get("rejection")),
                submission_call=submit["call"], evaluate_list_index=selected,
                recordings=[{k: v for k, v in rec.items() if k != "events"} for rec in recordings],
                declared_states=states, expected_declarations=expected,
                all_expected_declarations_observed=all(expected.values()), transitions=transitions, uav_barriers=uavs,
                transitions_recorded_later_on_cpu=inversions, fence_observation=fence,
                overlaps_at_checkpoint=[c["call"] for c in calls if c["entry"] < mark["order"] < c["returned"]],
                original_capture_qualified=False, projection=projection, gpu_safe=False, blockers=blockers)


def review_projection(producer, typed=False):
    require("projection" in producer, "projection observation missing")
    p = producer["projection"]
    require(p["attempted"] and not p["shader_execution_verified"] and not p["raster_jitter_verified"],
            "unsupported projection qualification")
    diagnostics = {}
    if typed:
        require(all(type(p.get(k)) is int and 0 <= p[k] <= 0xFFFFFFFFFFFFFFFF
                    for k in ("rejection", "vtable", "vtable_rva")), "projection type diagnostics missing")
        require(0 <= p["rejection"] <= 8 and (p["rejection"] == 0) == bool(p["valid"]), "projection rejection status")
        require(not p["valid"] or (p["vtable"] > p["vtable_rva"] and p["vtable_rva"] in (0x350D188, 0x33F82A8)),
                "projection camera type")
        reasons = ("none", "anchor", "read", "source_changed", "camera_type", "camera_epoch",
                   "buffer_unavailable", "layout", "changed_or_nonfinite")
        diagnostics = dict(rejection=p["rejection"], rejection_reason=reasons[p["rejection"]],
                           vtable=p["vtable"], vtable_rva=p["vtable_rva"])
    if not p["valid"]:
        require(not p["matrix_relation"], "invalid projection cannot match")
        return dict(valid=False, checkpoint=p["checkpoint"], matrix_relation=False,
                    shader_execution_verified=False, raster_jitter_verified=False, **diagnostics)
    require(p["checkpoint"] == 100 and 0 < p["reads"] <= 192 and p["epoch"] == producer["epoch"] & 0xFFFFFFFF and
            all(p[k] for k in ("manager", "metadata_table", "metadata", "constants")) and
            p["constants"] == p["tagged_constants"] & ~15 and 0 < p["words"] <= 4096 and p["buffer_id"] != 0xFFFFFFFF,
            "projection anchors")
    offsets = []
    for key in ("projection_id", "unjittered_id"):
        value = p[key]; offset = (value >> 12) & 1023
        require(0 <= value < 0xFFFFFFFF and (value >> 8) & 15 == 15 and offset + 16 <= min(1024, p["words"]),
                "projection parameter layout")
        offsets.append(offset)
    require(abs(offsets[0] - offsets[1]) >= 16, "overlapping projection parameters")
    def floats(key, size):
        bits = p[key]
        require(len(bits) == size and all(type(v) is int and 0 <= v <= 0xFFFFFFFF for v in bits), "projection float bits")
        values = list(struct.unpack("<" + "f" * size, struct.pack("<" + "I" * size, *bits)))
        require(all(math.isfinite(v) for v in values), "projection non-finite value")
        return values
    jitter, table20, viewport = (floats(k, 2) for k in ("jitter_bits", "table20_bits", "viewport_bits"))
    matrix, plain = (floats(k, 16) for k in ("projection_bits", "unjittered_bits"))
    require(all(v > 0 for v in viewport), "projection viewport")
    relation = any(plain[i] != 0 for i in (3, 7, 11, 15))
    errors = []
    for row in range(4):
        for col in range(4):
            a, b = matrix[4*row+col], plain[4*row+col]
            delta = plain[4*row+3] * jitter[col] / viewport[col] if col < 2 else 0
            error = abs((a - b) - delta); errors.append(error)
            relation = relation and error <= 8 * 2**-23 * max(1e-6, abs(a), abs(b), abs(delta))
    require(bool(p["matrix_relation"]) == relation, "projection transform result mismatch")
    rect = producer["rect"]
    return dict(valid=True, matrix_relation=relation, max_coefficient_error=max(errors),
                stored_camera_jitter=jitter, candidate_table20=table20, camera_matches_table20=p["jitter_bits"] == p["table20_bits"],
                viewport=viewport, viewport_matches_input=viewport == [rect[2]-rect[0], rect[3]-rect[1]],
                conditional_raster_offset=[jitter[0]/2, -jitter[1]/2] if relation else None,
                shader_execution_verified=False, raster_jitter_verified=False, **diagnostics)


def validate_publication_packet(packet, binding):
    require(packet["schema"] == 2 and packet["evaluate_bound"] and packet["consumer_bound"] and
            packet["publication_attempted"] and packet["publication_bound"] and packet["source_frame_link_verified"] and
            not packet["rejection"] and packet["stage_count"] == 5 and packet["command"] == binding["command"] and
            packet["resources"] == [binding["resources"][i] for i in (0, 1, 4, 3)], "publication packet binding")
    require(not any(packet[k] for k in ("temporal_alignment_verified", "shader_jitter_verified", "jitter_units_verified")),
            "unsupported temporal qualification")
    p, q, stages = packet["producer"], packet["publication"], packet["stages"]
    require(p["valid"] and q["valid"] and p["checkpoint"] == q["checkpoint"] == 100 and
            p["frame"] == p["frame_after"] and q["frame"] == q["frame_after"] and
            p["renderer"] == q["renderer"] and p["recording_context"] == q["recording_context"] == p["renderer"] + 0x710 and
            p["epoch"] == q["epoch"] and p["bank"] == q["bank"] and p["async_mode"] == q["async_mode"] and
            p["bank"] in (0, 1) and p["async_mode"] in (0, 1), "producer publication anchors")
    require(0 < q["count"] <= q["capacity"] <= 0x100000 and q["index"] == q["count"] - 1 and
            q["entry_base"] and q["entry_base"] % 16 == 0 and q["entry"] == q["entry_base"] + 16 * q["index"] and
            q["entry_base"] + 16 * q["count"] <= 0xFFFFFFFFFFFFFFFF and
            q["descriptor"] and q["payload"] and q["wrapper"] and len(q["owners"]) == 4 and all(q["owners"]),
            "publication entry and links")
    require(p["rect"] == [0, 0, 2560, 1440] and
            q["rect_bits"] == list(struct.unpack("<4I", struct.pack("<4f", *p["rect"]))) and
            q["reset_value_bits"] == p["reset_value_bits"] and len(stages) == 5, "publication payload scalars")
    require(p["current_index"] == p["frame"] % 3 and p["next_index"] == ((p["frame"] + 1) & 0xFFFFFFFFFFFFFFFF) % 3 and
            p["sharpen"] in (0, 1) and p["selected_index"] == p["next_index" if p["sharpen"] else "current_index"],
            "source history indices")
    for index, s in enumerate(stages):
        require(s["stage"] == index and s["valid"] and s["frame"] == s["frame_after"] and
                s["renderer"] == p["renderer"] and s["payload"] == q["payload"] and
                s["rect_bits"] == q["rect_bits"] and s["reset_value_bits"] == q["reset_value_bits"] and
                s["bank"] in (0, 1) and s["async_mode"] in (0, 1), "stage payload binding")
        if index < 4:
            require(s["descriptor"] == q["descriptor"] and s["context"] and s["context"] == stages[0]["context"] and
                    s["epoch"] == stages[0]["epoch"], "preparation receipt binding")
    # These counters are intentionally not required to equal the source counter.
    require(packet["evaluate_frame"] >= 0, "Evaluate frame observation")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path, required=True, help="Saved channels-<pid>.json with payload hashes")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    receipt = load_json(args.receipt)
    raw = {}
    for key in ("capture", "journal"):
        entry = receipt[key]
        require(Path(entry["name"]).name == entry["name"], "receipt payload must be a sibling file")
        raw[key] = (args.receipt.parent / entry["name"]).read_bytes()
        require(len(raw[key]) == entry["bytes"] and hashlib.sha256(raw[key]).hexdigest().upper() == entry["sha256"], "payload hash mismatch")
    journal = decode_journal(raw["journal"])
    require(journal["pid"] == receipt["pid"] and journal["creation"] == receipt["creation"], "process generation mismatch")
    require(receipt["capture_checksum_verified"] and receipt["inputs_verified"], "capture receipt unverified")
    capture = json.loads(raw["capture"], object_pairs_hook=unique_object)
    result = review(journal, capture, receipt["binding"])
    result["original_capture_qualified"] = receipt["qualified"]
    result["evidence_sha256"] = {key: receipt[key]["sha256"] for key in raw}
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
    print(json.dumps({key: result[key] for key in ("submission_call", "declared_states", "all_expected_declarations_observed", "fence_observation", "original_journal_flags", "gpu_safe")}, indent=2))


if __name__ == "__main__":
    main()

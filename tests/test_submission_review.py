"""Synthetic, application-independent counterexamples for offline replay."""
import copy
import json
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import review_submission as review


def fixture():
    resources = [100, 101, 0, 103, 104]
    def call(kind, command, entry, returned, **extra):
        return dict(command=command, caller=1, tick=100, thread=1, call=0, first=0, count=0,
                    kind=kind, result=0, queue=0, fence=0, value=0, entry=entry, returned=returned) | extra
    def transition(resource, before, after):
        return dict(type=0, flags=0, resource=resource, subresource=0xFFFFFFFF, before=before, after=after)
    calls = [
        call(1, 20, 1, 2), call(1, 30, 3, 4),
        call(0, 30, 5, 6, barriers=[transition(101, 8, 0x40)]),
        call(0, 30, 7, 8, barriers=[dict(type=2, flags=0, resource=104)]),
        call(0, 20, 10, 11, barriers=[transition(100, 4, 0x40), transition(101, 0, 8),
                                    transition(103, 0x400, 0xC0), transition(104, 0, 8)]),
        call(2, 20, 12, 13), call(2, 30, 14, 15),
        call(4, 0, 16, 17, queue=7, lists=[20, 30]),
        call(5, 0, 18, 19, queue=7, fence=88, value=3),
        call(7, 0, 20, 21, fence=88, value=2), call(7, 0, 22, 23, fence=88, value=3),
    ]
    for index, item in enumerate(calls, 1):
        item["call"] = index
    journal = dict(pid=7, creation=8, state=3, flags=0, pending=0, failed=0,
                   checkpoint=dict(command=30, tick=100, order=9, reserved=4, prefix=4), calls=calls)
    capture = dict(schema=1, producer=801, evaluate_index=0, committed=True, command_verified=True,
                   boundary_clean=True, handle_record_verified=True, handle_binding_matched=True,
                   thread=1, tick=100, contract=dict(complete=True, fault=False, queries=17,
                   resources=[dict(key=i, value=v, attempted=True, completed=True, present=True, result=1, null=not v)
                              for i, v in enumerate(resources)]),
                   inspection=dict(complete=True, fault=False), barriers=dict(command=30))
    binding = dict(command=30, thread=1, index=0, resources=resources)
    return journal, capture, binding


def encode(journal):
    """Build synthetic producer-801 bytes with no game or captured memory."""
    data = bytearray(753832)
    struct.pack_into("<QIIIIQ", data, 0, 0x314C4E4A4457484D, 1, 801, len(data), journal["pid"], journal["creation"])
    mark = journal["checkpoint"]
    struct.pack_into("<4I4Q2I", data, 32, journal["state"], journal["flags"], 2, 0, 999,
                     mark["command"], mark["tick"], mark["order"], mark["reserved"], mark["prefix"])
    next_barrier = next_list = 0
    for call in copy.deepcopy(journal["calls"]):
        payload = bytearray()
        if call["kind"] == 0:
            call["first"], call["count"] = next_barrier, len(call["barriers"])
            for barrier in call["barriers"]:
                raw = bytearray(32)
                struct.pack_into("<IIQ", raw, 0, barrier["type"], barrier["flags"], barrier.get("resource", 0))
                if barrier["type"] == 0:
                    struct.pack_into("<III", raw, 16, barrier["subresource"], barrier["before"], barrier["after"])
                payload.extend(raw)
            start = 88 + 2048 * 96 + next_barrier * 32
            data[start:start + len(payload)] = payload
            next_barrier += call["count"]
        elif call["kind"] == 4:
            call["first"], call["count"] = next_list, len(call["lists"])
            payload = struct.pack("<" + "Q" * call["count"], *call["lists"])
            start = 88 + 2048 * 96 + 16384 * 32 + next_list * 8
            data[start:start + len(payload)] = payload
            next_list += call["count"]
        raw = struct.pack("<3Q6I5Q", *[call[k] for k in "command caller tick thread call first count kind result queue fence value entry returned".split()])
        offset = 88 + (call["call"] - 1) * 96
        struct.pack_into("<II", data, offset, 2, review.fnv32(raw + payload))
        data[offset + 8:offset + 96] = raw
    return data


class SubmissionReviewTests(unittest.TestCase):
    def setUp(self):
        self.journal, self.capture, self.binding = fixture()
        self.calls = self.journal["calls"]

    def run_review(self):
        return review.review(self.journal, self.capture, self.binding)

    def rejected(self, pattern):
        with self.assertRaisesRegex(review.ReviewError, pattern):
            self.run_review()

    def publication_fixture(self):
        self.journal["producer"] = self.capture["producer"] = 1200
        self.capture["has_frame_packet"] = True
        rect = list(struct.unpack("<4I", struct.pack("<4f", 0, 0, 2560, 1440)))
        p = dict(valid=True, checkpoint=100, renderer=10000, recording_context=10000+0x710,
                 frame=6, frame_after=6, epoch=7, bank=0, async_mode=1, rect=[0, 0, 2560, 1440],
                 reset_value_bits=0, current_index=0, next_index=1, selected_index=0, sharpen=0)
        q = dict(valid=True, checkpoint=100, renderer=10000, recording_context=10000+0x710,
                 frame=6, frame_after=6, epoch=7, bank=0, async_mode=1, rect_bits=rect, reset_value_bits=0,
                 entry_base=1600, entry=1632, index=2, count=3, capacity=8, descriptor=77, payload=99,
                 wrapper=44, owners=[1, 2, 3, 4])
        stages = [dict(stage=i, valid=True, renderer=10000, frame=17+i, frame_after=17+i, renderer_epoch=28,
                       bank=1, async_mode=1, payload=99, descriptor=77 if i<4 else 0, context=66 if i<4 else 0,
                       epoch=28 if i<4 else 0, rect_bits=rect[:], reset_value_bits=0) for i in range(5)]
        packet = dict(schema=2, evaluate_bound=True, consumer_bound=True, publication_attempted=True,
                      publication_bound=True, source_frame_link_verified=True, rejection=0, stage_count=5,
                      command=30, resources=[100, 101, 104, 103], temporal_alignment_verified=False,
                      shader_jitter_verified=False, jitter_units_verified=False, producer=p, publication=q,
                      stages=stages, evaluate_frame=49)
        self.capture["frame_packet"] = packet
        return packet

    def test_publication_accepts_separate_counters(self):
        self.publication_fixture()
        self.assertTrue(self.run_review()["frame_packet_bound"])
        self.assertFalse(self.run_review()["gpu_safe"])

    def projection_fixture(self):
        packet = self.publication_fixture()
        self.journal["producer"] = self.capture["producer"] = 1300
        def bits(values): return list(struct.unpack("<"+"I"*len(values), struct.pack("<"+"f"*len(values), *values)))
        plain = [float(i//4 == i%4) for i in range(16)]
        matrix = plain.copy(); matrix[12] = .25/2560; matrix[13] = -.125/1440
        p = dict(attempted=True, valid=True, matrix_relation=True, checkpoint=100, reads=120, epoch=7,
                 manager=100, metadata_table=200, metadata=300, constants=1600, tagged_constants=1603,
                 words=32, buffer_id=1, projection_id=0xf00, unjittered_id=0x10f00,
                 jitter_bits=bits([.25,-.125]), table20_bits=bits([.25,-.125]), viewport_bits=bits([2560,1440]),
                 projection_bits=bits(matrix), unjittered_bits=bits(plain), shader_execution_verified=False, raster_jitter_verified=False)
        packet["producer"]["projection"] = p
        return p

    def test_projection_relation_is_independently_replayed(self):
        self.projection_fixture()
        result = self.run_review()
        self.assertTrue(result["projection"]["matrix_relation"])
        self.assertEqual(result["projection"]["conditional_raster_offset"], [.125,.0625])
        self.assertFalse(result["projection"]["raster_jitter_verified"])
        self.assertFalse(result["gpu_safe"])

    def test_projection_rejects_invalid_claims_and_layouts(self):
        changes = [("epoch", 8), ("reads", 193), ("constants", 1601), ("projection_id", 0x3fff00),
                   ("unjittered_id", 0xf00), ("words", 31), ("matrix_relation", False),
                   ("shader_execution_verified", True), ("raster_jitter_verified", True)]
        for key, value in changes:
            with self.subTest(key=key):
                p = self.projection_fixture(); p[key] = value
                with self.assertRaises(review.ReviewError): self.run_review()

    def test_projection_missing_or_nonfinite_is_rejected(self):
        self.projection_fixture(); del self.capture["frame_packet"]["producer"]["projection"]
        self.rejected("projection observation missing")
        for key in ("jitter_bits", "table20_bits", "viewport_bits", "projection_bits", "unjittered_bits"):
            p = self.projection_fixture(); p[key][0] = 0x7fc00000
            self.rejected("non-finite")

    def test_projection_failed_observation_preserves_cpu_receipt(self):
        p = self.projection_fixture(); p.update(valid=False, matrix_relation=False, checkpoint=3)
        result = self.run_review()
        self.assertTrue(result["frame_packet_bound"])
        self.assertFalse(result["projection"]["valid"])

    def test_projection_mismatch_and_other_sequence_are_observations(self):
        p = self.projection_fixture(); p["projection_bits"][12] = 0; p["matrix_relation"] = False
        p["table20_bits"][0] = 0
        result = self.run_review()["projection"]
        self.assertFalse(result["matrix_relation"])
        self.assertFalse(result["camera_matches_table20"])
        self.assertIsNone(result["conditional_raster_offset"])

    def test_projection_producer_keeps_journal_layout(self):
        for producer in (1300, 1301, 1400, 1500):
            data = encode(self.journal); struct.pack_into("<III", data, 8, 2, producer, 753944); data += bytes(112)
            self.assertEqual(review.decode_journal(data)["producer"], producer)

    def typed_projection_fixture(self):
        p = self.projection_fixture()
        self.journal["producer"] = self.capture["producer"] = 1301
        p.update(rejection=0, vtable=0x140000000+0x33F82A8, vtable_rva=0x33F82A8)
        return p

    def test_projection_type_accepts_only_verified_base_or_derived(self):
        p = self.typed_projection_fixture()
        for rva in (0x350D188, 0x33F82A8):
            p.update(vtable=0x140000000+rva, vtable_rva=rva)
            self.assertTrue(self.run_review()["projection"]["valid"])
        p["vtable_rva"] += 8
        self.rejected("camera type")

    def test_projection_type_failure_keeps_observed_type(self):
        p = self.typed_projection_fixture()
        p.update(valid=False, matrix_relation=False, checkpoint=2, reads=14, rejection=4, vtable_rva=0x1234)
        result = self.run_review()
        self.assertTrue(result["frame_packet_bound"])
        self.assertEqual(result["projection"]["rejection_reason"], "camera_type")
        self.assertEqual(result["projection"]["vtable_rva"], 0x1234)

    def test_projection_type_missing_diagnostics_rejected(self):
        for key in ("rejection", "vtable", "vtable_rva"):
            p = self.typed_projection_fixture(); del p[key]
            self.rejected("diagnostics missing")

    def test_projection_type_rejection_status_consistency(self):
        p = self.typed_projection_fixture(); p["rejection"] = 4
        self.rejected("rejection status")
        p.update(valid=False, matrix_relation=False, rejection=0)
        self.rejected("rejection status")

    def test_publication_rejects_broken_source_and_stage_links(self):
        for field in ("descriptor", "payload", "renderer", "epoch", "recording_context", "entry", "index", "reset_value_bits"):
            with self.subTest(field=field):
                packet = self.publication_fixture()
                packet["publication"][field] += 1
                with self.assertRaises(review.ReviewError): self.run_review()
        for field in ("descriptor", "payload", "context", "epoch", "renderer", "reset_value_bits", "frame_after"):
            with self.subTest(stage_field=field):
                packet = self.publication_fixture()
                packet["stages"][2][field] += 1
                with self.assertRaises(review.ReviewError): self.run_review()

    def test_publication_cannot_claim_temporal_validation(self):
        for field in ("temporal_alignment_verified", "shader_jitter_verified", "jitter_units_verified"):
            packet = self.publication_fixture(); packet[field] = True
            self.rejected("unsupported temporal")

    def test_publication_required_when_declared(self):
        self.publication_fixture(); self.capture.pop("frame_packet")
        self.rejected("presence")
        packet = self.publication_fixture(); packet["publication_bound"] = False
        self.rejected("publication packet")

    def test_new_producer_keeps_journal_layout(self):
        data = encode(self.journal)
        struct.pack_into("<III", data, 8, 2, 1200, 753944)
        data += bytes(112)
        decoded = review.decode_journal(data)
        self.assertEqual(decoded["producer"], 1200)
        self.assertFalse(decoded["drained"])

    def test_recording_order_resolves_late_cpu_transitions(self):
        result = self.run_review()
        self.assertEqual(result["declared_states"], {0: 0x40, 1: 0x40, 3: 0xC0, 4: 8})
        self.assertEqual(len(result["transitions_recorded_later_on_cpu"]), 4)
        self.assertEqual(result["fence_observation"]["completed_call"], 11)
        self.assertFalse(result["gpu_safe"])

    def test_overflow_is_preserved_after_useful_chain(self):
        self.journal["flags"] = 1
        result = self.run_review()
        self.assertEqual(result["original_journal_flags"], 1)
        self.assertIn("journal flags", result["blockers"][0])
        self.assertFalse(result["gpu_safe"])

    def test_missing_reset(self):
        self.calls[0]["kind"] = 0
        self.rejected("no observed Reset")

    def test_failed_reset(self):
        self.calls[0]["result"] = 0x80004005
        self.rejected("Reset failed")

    def test_failed_close(self):
        self.calls[5]["result"] = 0x80004005
        self.rejected("Close")

    def test_missing_close(self):
        self.calls[5]["kind"] = 0
        self.rejected("Close")

    def test_reused_list_does_not_match_old_evaluate(self):
        for call in self.calls:
            call["entry"] *= 10
            call["returned"] *= 10
        self.journal["checkpoint"]["order"] *= 10
        self.calls[1]["entry"], self.calls[1]["returned"] = 91, 92
        self.rejected("reset or closed")

    def test_callback_crosses_evaluate_on_selected_list(self):
        for call in self.calls:
            call["entry"] *= 10
            call["returned"] *= 10
        self.journal["checkpoint"]["order"] *= 10
        self.calls[3]["returned"] = 95
        self.rejected("overlaps selected")

    def test_overlapping_recording_callbacks(self):
        for call in self.calls:
            call["entry"] *= 10
            call["returned"] *= 10
        self.journal["checkpoint"]["order"] *= 10
        self.calls[2]["returned"] = 75
        self.rejected("concurrent recording")

    def test_bundle_is_not_silently_ignored(self):
        self.calls[3]["kind"] = 3
        self.rejected("bundle")

    def test_close_overlaps_submission(self):
        self.calls[6]["returned"] = 24
        self.rejected("overlaps submission")

    def test_duplicate_submission_list(self):
        self.calls[7]["lists"] = [20, 30, 30]
        self.rejected("invalid submission")

    def test_uav_barrier_does_not_establish_output_state(self):
        self.calls[4]["barriers"].pop()
        result = self.run_review()
        self.assertIsNone(result["declared_states"][4])
        self.assertFalse(result["all_expected_declarations_observed"])

    def test_split_barrier(self):
        self.calls[4]["barriers"][0]["flags"] = 1
        self.rejected("split")

    def test_aliasing_barrier(self):
        self.calls[4]["barriers"].append(dict(type=1, flags=0))
        self.rejected("aliasing")

    def test_partial_subresource(self):
        self.calls[4]["barriers"][0]["subresource"] = 0
        self.rejected("subresource")

    def test_discontinuous_state(self):
        self.calls[2]["barriers"][0]["before"] = 4
        self.rejected("discontinuity")

    def test_resource_binding_mismatch(self):
        self.binding["resources"][0] = 999
        self.rejected("binding differs")

    def test_failed_capture(self):
        self.capture["contract"]["fault"] = True
        self.rejected("incomplete")

    def test_wrong_checkpoint(self):
        self.binding["command"] = 99
        self.rejected("checkpoint binding")

    def test_capture_published_after_marker_in_same_bounded_window(self):
        self.capture["tick"] += 2
        self.assertTrue(self.run_review()["all_expected_declarations_observed"])
        self.capture["tick"] += 2000
        self.rejected("checkpoint binding")

    def test_sequence_hole(self):
        del self.calls[4]
        self.rejected("sequence hole")

    def test_pending_slot(self):
        self.journal["pending"] = 1
        self.rejected("pending")

    def test_duplicate_event_order(self):
        self.calls[4]["entry"] = 9
        self.rejected("duplicate event")

    def test_cpu_signal_is_not_gpu_completion(self):
        self.calls[8]["kind"] = 8
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_signal_on_other_queue(self):
        self.calls[8]["queue"] = 8
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_failed_signal(self):
        self.calls[8]["result"] = 0x80004005
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_completion_value_too_small(self):
        self.calls[10]["value"] = 2
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_device_removed_fence_value(self):
        self.calls[9]["value"] = (1 << 64) - 1
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_competing_fence_writer(self):
        self.calls[9].update(kind=5, queue=8, value=4)
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_rewound_fence_signal(self):
        self.calls[9].update(kind=5, queue=7, value=2)
        self.assertIsNone(self.run_review()["fence_observation"])

    def test_queue_call_started_before_submission_and_overlaps_it(self):
        for call in self.calls:
            call["entry"] *= 10
            call["returned"] *= 10
        self.journal["checkpoint"]["order"] *= 10
        self.calls[9].update(kind=6, queue=7, entry=155, returned=165)
        result = self.run_review()
        self.assertIsNone(result["fence_observation"])
        self.assertIn("overlapping queue calls", result["blockers"])

    def test_binary_round_trip(self):
        decoded = review.decode_journal(encode(self.journal))
        self.assertEqual(review.review(decoded, self.capture, self.binding)["declared_states"], self.run_review()["declared_states"])

    def test_binary_corruption_rejected(self):
        for offset in (0, 8, 12, 88 + 8, 88 + 2048 * 96 + 8):
            with self.subTest(offset=offset):
                data = encode(self.journal)
                data[offset] ^= 1
                with self.assertRaises(review.ReviewError):
                    review.decode_journal(data)

    def test_version_two_terminal_checksum_and_frontier(self):
        data = encode(self.journal)
        struct.pack_into("<III", data, 8, 2, 1100, 753944)
        completion = struct.pack("<7I4x6Q", 1, 0, 2, 7, 8, 9, 11, 30, 7, 88, 9, 3, 23)
        terminal = struct.pack("<4IQ", 2, 0, 11, 0, 24) + completion
        data += struct.pack("<II", 2, review.fnv32(terminal)) + terminal
        decoded = review.decode_journal(data)
        self.assertTrue(decoded["drained"])
        self.assertEqual(decoded["termination"]["completed"], 11)
        data[-1] ^= 1
        with self.assertRaisesRegex(review.ReviewError, "terminal checksum"):
            review.decode_journal(data)

    def test_version_two_admission_closed_but_not_drained(self):
        data = encode(self.journal)
        struct.pack_into("<III", data, 8, 2, 1100, 753944)
        data += bytes(112)
        self.assertFalse(review.decode_journal(data)["drained"])

    def test_duplicate_json_keys_rejected(self):
        with self.assertRaises(review.ReviewError):
            json.loads('{"state":1,"state":0}', object_pairs_hook=review.unique_object)


if __name__ == "__main__":
    unittest.main()

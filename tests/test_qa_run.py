#!/usr/bin/env python3
"""Headless tests for qa_record and tools/qa_run.py; needs no display server and no ffmpeg.

Usage: test_qa_run.py [path/to/qa_record] [unittest args...]
"""

from __future__ import annotations

import importlib.util
import json
import os
import signal
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parent.parent
QA_RUN_PATH = ROOT / "tools" / "qa_run.py"
SCENARIOS = ["lobby_brush", "thunderburst_stress", "act1_combat", "boss_showcase"]
SAMPLES_PER_TICK = 735
SMOKE_FRAMES = 30
CHILD_TIMEOUT = 300

QA_RECORD = ROOT / "build" / "debug" / "qa_record"


def load_qa_run():
    spec = importlib.util.spec_from_file_location("qa_run", QA_RUN_PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


qa_run = load_qa_run()


def headless_env() -> dict:
    env = dict(os.environ)
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    return env


def run_qa_record(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(QA_RECORD), *args],
        capture_output=True,
        text=True,
        env=headless_env(),
        timeout=CHILD_TIMEOUT,
        check=False,
    )


def make_report(name: str, **sections) -> dict:
    report = {
        "scenario": name,
        "frames": 540,
        "fps": 60,
        "width": 1280,
        "height": 720,
        "bgm_enabled": True,
        "video_enabled": True,
        "post_enabled": True,
        "fluid_gpu": True,
        "determinism": {
            "final_hash": "0123456789abcdef",
            "fluid_finite": True,
            "audio_finite": True,
            "dropped_events": 0,
        },
        "video": {"black_frames": 0, "white_frames": 0, "static_frames": 0, "mean_luma": 180.0},
        "audio": {
            "se_peak": 28000,
            "bgm_peak": 13000,
            "mix_peak": 28000,
            "clip_ratio": 0.0,
            "bgm_tracks_loaded": 4,
        },
        "vfx": {
            "max_shake_amp": 9.0,
            "thunderburst_max_shake_amp": 4.2,
            "shake_active_ratio": 0.31,
            "max_callouts_active": 8,
        },
        "game": {"thunderbursts": 271, "kills": 18},
    }
    for section, values in sections.items():
        report[section] = {**report[section], **values} if isinstance(values, dict) else values
    return report


def failed_checks(name: str, report: dict, *argv: str) -> list[str]:
    args = qa_run.parse_args(["--scenario", name, *argv])
    return [c["check"] for c in qa_run.evaluate(name, report, args) if not c["ok"]]


class QaRecordHeadlessTest(unittest.TestCase):
    def setUp(self) -> None:
        tmp = tempfile.TemporaryDirectory(prefix="inkstorm-qa-test-")
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)

    def test_lists_exactly_the_four_scenarios(self) -> None:
        proc = run_qa_record("--list-scenarios")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(proc.stdout.split(), SCENARIOS)

    def test_no_render_run_writes_exact_wav_and_report_without_display(self) -> None:
        wav = self.tmp / "audio.wav"
        report_path = self.tmp / "report.json"
        proc = run_qa_record(
            "--scenario", "lobby_brush", "--frames", str(SMOKE_FRAMES), "--no-render",
            "--audio-out", str(wav), "--report-out", str(report_path),
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)

        data = wav.read_bytes()
        data_bytes = SMOKE_FRAMES * SAMPLES_PER_TICK * 2
        self.assertEqual(len(data), 44 + data_bytes)
        riff, riff_size, wave = struct.unpack_from("<4sI4s", data, 0)
        fmt_id, fmt_len, tag, channels, rate, byte_rate, align, bits = struct.unpack_from(
            "<4sIHHIIHH", data, 12
        )
        data_id, data_len = struct.unpack_from("<4sI", data, 36)
        self.assertEqual((riff, wave, fmt_id, data_id), (b"RIFF", b"WAVE", b"fmt ", b"data"))
        self.assertEqual((riff_size, data_len), (36 + data_bytes, data_bytes))
        self.assertEqual((fmt_len, tag, channels, rate), (16, 1, 1, 44100))
        self.assertEqual((byte_rate, align, bits), (88200, 2, 16))

        report = json.loads(report_path.read_text())
        self.assertEqual(report["frames"], SMOKE_FRAMES)
        self.assertIs(report["video_enabled"], False)
        self.assertIs(report["post_enabled"], False)
        self.assertIs(report["determinism"]["audio_finite"], True)
        self.assertIs(report["determinism"]["fluid_finite"], True)
        self.assertEqual(report["audio"]["bgm_tracks_loaded"], 4)
        self.assertGreaterEqual(report["audio"]["bgm_peak"], 500)
        self.assertGreaterEqual(report["audio"]["se_peak"], 500)

    def test_aliased_outputs_are_rejected_without_clobbering(self) -> None:
        target = self.tmp / "keep.txt"
        target.write_text("keep")
        link = self.tmp / "link.txt"
        link.symlink_to(target)
        for audio_out in (target, link):
            proc = run_qa_record(
                "--scenario", "lobby_brush", "--frames", "1", "--no-render",
                "--audio-out", str(audio_out), "--report-out", str(target),
            )
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("different files", proc.stderr)
            self.assertEqual(target.read_text(), "keep")

    def test_fifo_audio_output_is_rejected_instead_of_blocking(self) -> None:
        fifo = self.tmp / "audio.fifo"
        os.mkfifo(fifo)
        proc = run_qa_record(
            "--scenario", "lobby_brush", "--frames", "1", "--no-render", "--audio-out", str(fifo)
        )
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("regular file", proc.stderr)

    def test_qa_run_no_video_run_passes_and_publishes(self) -> None:
        out = self.tmp / "out"
        proc = subprocess.run(
            [
                sys.executable, str(QA_RUN_PATH), "--scenario", "lobby_brush",
                "--frames", str(SMOKE_FRAMES), "--no-video", "--bin", str(QA_RECORD),
                "--out", str(out),
            ],
            capture_output=True,
            text=True,
            env=headless_env(),
            timeout=CHILD_TIMEOUT,
            check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        summary = json.loads((out / "summary.json").read_text())
        self.assertIs(summary["passed"], True)
        report = json.loads((out / "lobby_brush" / "report.json").read_text())
        self.assertEqual(report["frames"], SMOKE_FRAMES)
        self.assertTrue((out / "lobby_brush" / "audio.wav").is_file())


class QaRunPathSafetyTest(unittest.TestCase):
    def setUp(self) -> None:
        tmp = tempfile.TemporaryDirectory(prefix="inkstorm-qa-test-")
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.base = self.tmp / "base"
        self.base.mkdir()
        self.outside = self.tmp / "outside"
        self.outside.mkdir()

    def test_symlinked_out_dir_is_rejected(self) -> None:
        link = self.tmp / "out-link"
        link.symlink_to(self.outside)
        with self.assertRaises(qa_run.PipelineError):
            qa_run.prepare_out_dir(link, allow_in_repo=False)

    def test_symlinked_scenario_dirs_are_rejected(self) -> None:
        (self.base / "lobby_brush").symlink_to(self.outside)
        with self.assertRaises(qa_run.PipelineError):
            qa_run.ensure_safe_dir(self.base, ("lobby_brush",))
        (self.base / "act1_combat").mkdir()
        (self.base / "act1_combat" / "snaps").symlink_to(self.outside)
        with self.assertRaises(qa_run.PipelineError):
            qa_run.ensure_safe_dir(self.base, ("act1_combat", "snaps"))
        with self.assertRaises(qa_run.PipelineError):
            qa_run.ensure_safe_dir(self.base, ("..",))
        self.assertEqual(list(self.outside.iterdir()), [])

    def test_symlinked_summary_is_not_written_through(self) -> None:
        victim = self.outside / "victim.txt"
        victim.write_text("keep")
        (self.base / "summary.json").symlink_to(victim)
        with self.assertRaises(qa_run.PipelineError):
            qa_run.atomic_publish_bytes(b"{}", self.base / "summary.json", self.base.resolve())
        self.assertEqual(victim.read_text(), "keep")

    def test_publish_replaces_regular_file_atomically(self) -> None:
        dst = self.base / "summary.json"
        dst.write_text("old")
        qa_run.atomic_publish_bytes(b"new", dst, self.base.resolve())
        self.assertEqual(dst.read_text(), "new")
        self.assertEqual(sorted(p.name for p in self.base.iterdir()), ["summary.json"])


class QaRunReportTest(unittest.TestCase):
    def test_malformed_reports_raise_pipeline_errors(self) -> None:
        bad_reports = ["[]", "{", '{"frames": NaN}', '{"frames": Infinity}', '{"frames": "30"}',
                       '{"frames": 30.5}', '{"frames": true}', '{"frames": 0}']
        with tempfile.TemporaryDirectory(prefix="inkstorm-qa-test-") as tmp:
            path = Path(tmp) / "report.json"
            for text in bad_reports:
                with self.subTest(report=text):
                    path.write_text(text)
                    with self.assertRaises(qa_run.PipelineError):
                        qa_run.load_report(path)

    def test_malformed_report_from_recorder_fails_the_scenario(self) -> None:
        with tempfile.TemporaryDirectory(prefix="inkstorm-qa-test-") as tmp:
            fake = Path(tmp) / "fake_qa_record"
            fake.write_text(
                "#!/bin/sh\n"
                "while [ $# -gt 0 ]; do\n"
                '  case "$1" in\n'
                "    --report-out) printf '{\"frames\": NaN}' > \"$2\"; shift ;;\n"
                '    --audio-out) : > "$2"; shift ;;\n'
                "  esac\n"
                "  shift\n"
                "done\n"
            )
            fake.chmod(0o700)
            out = Path(tmp) / "out"
            out.mkdir()
            args = qa_run.parse_args(["--scenario", "lobby_brush", "--no-video"])
            result = qa_run.run_scenario("lobby_brush", fake, out, args)
        self.assertIs(result["passed"], False)
        self.assertIn("malformed report", result["error"])

    def test_post_fallback_mismatch_fails(self) -> None:
        report = make_report("lobby_brush", vfx={"max_shake_amp": 0.0}, post_enabled=False)
        self.assertEqual(failed_checks("lobby_brush", report), ["post_enabled == true"])
        self.assertEqual(failed_checks("lobby_brush", report, "--no-post"), [])

    def test_non_finite_metric_values_fail(self) -> None:
        report = make_report("act1_combat", audio={"clip_ratio": float("nan")})
        self.assertIn("audio.clip_ratio < 0.01", failed_checks("act1_combat", report))

    def test_thunderburst_shake_is_judged_apart_from_cataclysm(self) -> None:
        name = "thunderburst_stress"
        self.assertEqual(failed_checks(name, make_report(name)), [])
        cases = {
            "vfx.thunderburst_max_shake_amp > 0": {"thunderburst_max_shake_amp": 0.0},
            "vfx.thunderburst_max_shake_amp <= 4.5": {"thunderburst_max_shake_amp": 5.5},
            "vfx.shake_active_ratio <= 0.45": {"shake_active_ratio": 0.5},
        }
        for check, vfx in cases.items():
            with self.subTest(check=check):
                self.assertEqual(failed_checks(name, make_report(name, vfx=vfx)), [check])

    def test_no_video_skips_video_rules_but_keeps_audio_and_game_rules(self) -> None:
        report = make_report(
            "thunderburst_stress",
            video_enabled=False,
            post_enabled=False,
            video={"black_frames": 540},
            vfx={"thunderburst_max_shake_amp": 0.0},
            game={"thunderbursts": 0},
        )
        self.assertEqual(
            failed_checks("thunderburst_stress", report, "--no-video"),
            ["game.thunderbursts >= 10"],
        )


class QaRunSupervisionTest(unittest.TestCase):
    def test_supervise_timeout_reaps_stalled_process_groups(self) -> None:
        procs = [qa_run.popen(["sh", "-c", "exec sleep 60"]) for _ in range(2)]
        for proc in procs:
            self.addCleanup(qa_run.stop_process, proc)
        with self.assertRaises(qa_run.PipelineError):
            qa_run.supervise(procs[0], procs[1], timeout=0.3)
        for proc in procs:
            self.assertIsNotNone(proc.returncode)
            deadline = time.monotonic() + 5.0
            while True:
                try:
                    os.killpg(proc.pid, 0)
                except ProcessLookupError:
                    break
                self.assertLess(time.monotonic(), deadline, "process group still alive")
                time.sleep(0.05)

    def test_run_bounded_timeout_reaps_descendant_holding_pipe(self) -> None:
        started = time.monotonic()
        with self.assertRaises(qa_run.PipelineError):
            qa_run.run_bounded(
                ["sh", "-c", "sleep 60 & exit 0"],
                timeout=0.3,
                what="orphan-pipe",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
        self.assertLess(time.monotonic() - started, 5.0)

    def test_failed_recorder_stops_encoder_waiting_on_fifo(self) -> None:
        recorder = qa_run.popen(["sh", "-c", "exit 3"])
        encoder = qa_run.popen(["sh", "-c", "exec sleep 60"])
        self.addCleanup(qa_run.stop_process, encoder)
        rec_rc, enc_rc, encoder_first = qa_run.supervise(recorder, encoder, timeout=30.0)
        self.assertEqual((rec_rc, encoder_first), (3, False))
        self.assertEqual(enc_rc, -signal.SIGTERM)


def main() -> None:
    global QA_RECORD
    argv = sys.argv[1:]
    if argv and not argv[0].startswith("-"):
        QA_RECORD = Path(argv.pop(0)).resolve()
    if not (QA_RECORD.is_file() and os.access(QA_RECORD, os.X_OK)):
        sys.exit(f"qa_record not found: {QA_RECORD}")
    unittest.main(argv=[sys.argv[0], *argv], verbosity=2)


if __name__ == "__main__":
    main()

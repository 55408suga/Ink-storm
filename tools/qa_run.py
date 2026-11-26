#!/usr/bin/env python3
"""Record INKSTORM QA scenarios offscreen with qa_record and check their A/V metrics.

Each scenario publishes <out>/<scenario>/{<scenario>.mp4, report.json, snaps/} and the run
publishes <out>/summary.json. Recording happens in a private work directory; results are copied
into --out and renamed into place atomically. Exit status is 0 only when every scenario records
and passes its checks.
"""

from __future__ import annotations

import argparse
import contextlib
import fcntl
import json
import math
import operator
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BINS = (ROOT / "build" / "release" / "qa_record", ROOT / "build" / "debug" / "qa_record")
EXTRACT_SNAPS = ROOT / "tools" / "extract_snaps.sh"
VIDEO_WIDTH = 1280
VIDEO_HEIGHT = 720
VIDEO_FPS_INT = 60
VIDEO_SIZE = f"{VIDEO_WIDTH}x{VIDEO_HEIGHT}"
VIDEO_FPS = str(VIDEO_FPS_INT)
MAX_FRAMES = VIDEO_FPS_INT * 60 * 60
SNAP_COUNT = 6
POLL_SECONDS = 0.05
STOP_TIMEOUT_SECONDS = 5.0
LIST_TIMEOUT_SECONDS = 15.0
STEP_TIMEOUT_SECONDS = 120.0
MIN_RECORD_TIMEOUT_SECONDS = 120.0
LOG_TAIL_LINES = 20
SCENARIO_NAME = re.compile(r"[a-z0-9][a-z0-9_-]*")
SNAP_FILE = re.compile(r"snap_[0-9]+\.png")

OPS = {
    "==": operator.eq,
    ">": operator.gt,
    "<": operator.lt,
    "<=": operator.le,
    ">=": operator.ge,
}


class PipelineError(Exception):
    pass


def default_out_dir() -> Path:
    base = Path(tempfile.gettempdir())
    if any(part.startswith("nix-shell.") for part in base.parts):
        base = Path("/tmp")
    return base / f"inkstorm-qa-{os.getuid()}"


def positive_int(text: str) -> int:
    try:
        value = int(text)
    except ValueError:
        raise argparse.ArgumentTypeError(f"not an integer: {text!r}") from None
    if value <= 0 or value > MAX_FRAMES:
        raise argparse.ArgumentTypeError(f"must be in 1..{MAX_FRAMES}: {value}")
    return value


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Record INKSTORM QA scenarios (video + audio) and check their metrics."
    )
    parser.add_argument(
        "--scenario",
        action="append",
        default=[],
        metavar="NAME",
        help="scenario to run (repeatable)",
    )
    parser.add_argument("--all", action="store_true", help="run every scenario qa_record lists")
    parser.add_argument(
        "--out",
        type=Path,
        help="output directory (default: a private inkstorm-qa-<uid> in the temp directory)",
    )
    parser.add_argument(
        "--allow-in-repo", action="store_true", help="allow --out inside the git repository"
    )
    parser.add_argument(
        "--bin", type=Path, help="qa_record path (default build/release, then build/debug)"
    )
    parser.add_argument("--frames", type=positive_int, help="override each scenario's frame count")
    parser.add_argument("--no-bgm", action="store_true", help="record SE and brush audio only")
    parser.add_argument("--no-post", action="store_true", help="disable the post-process shader")
    parser.add_argument(
        "--no-video",
        action="store_true",
        help="headless audio + report run (no window, video, or snapshots)",
    )
    parser.add_argument("--keep-wav", action="store_true", help="keep audio.wav beside the mp4")
    args = parser.parse_args(argv)
    if not args.all and not args.scenario:
        parser.print_usage(sys.stderr)
        print(f"{parser.prog}: error: pass --scenario NAME or --all", file=sys.stderr)
        sys.exit(2)
    return args


def repo_root() -> Path:
    try:
        top = subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "--show-toplevel"],
            check=True,
            capture_output=True,
            text=True,
            timeout=LIST_TIMEOUT_SECONDS,
        ).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ROOT
    return Path(top).resolve() if top else ROOT


def check_private_dir(path: Path) -> None:
    if path.is_symlink():
        raise PipelineError(f"{path} is a symlink")
    if not path.is_dir():
        raise PipelineError(f"{path} is not a directory")
    st = path.stat()
    if st.st_uid != os.getuid():
        raise PipelineError(f"{path} is not owned by the current user")
    if (st.st_mode & 0o022) != 0:
        raise PipelineError(f"{path} must not be group- or world-writable")


@contextlib.contextmanager
def publish_lock(base: Path):
    lock_path = base / ".qa_run.lock"
    if lock_path.is_symlink() or lock_path.is_dir():
        raise PipelineError(f"refusing to lock {lock_path}: not a regular file")
    fd = os.open(lock_path, os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        fcntl.flock(fd, fcntl.LOCK_EX)
        yield
    finally:
        try:
            fcntl.flock(fd, fcntl.LOCK_UN)
        finally:
            os.close(fd)


def prepare_out_dir(out: Path | None, allow_in_repo: bool) -> Path:
    path = Path(os.path.abspath((out if out is not None else default_out_dir()).expanduser()))
    if path.is_symlink():
        raise PipelineError(f"--out {path} is a symlink")
    resolved = path.resolve()
    root = repo_root()
    if not allow_in_repo and resolved.is_relative_to(root):
        raise PipelineError(
            f"--out {resolved} is inside the repository {root}; "
            "pick a directory outside it or pass --allow-in-repo"
        )
    try:
        path.mkdir(mode=0o700, parents=True)
    except FileExistsError:
        pass
    check_private_dir(path)
    return path.resolve()


def ensure_safe_dir(base: Path, rel_parts: tuple[str, ...]) -> Path:
    """Create or walk base/<rel_parts> one component at a time, refusing symlinks and escapes."""
    base_resolved = base.resolve()
    current = base_resolved
    for part in rel_parts:
        if part in ("", ".", "..") or "/" in part:
            raise PipelineError(f"unsafe path component {part!r}")
        current = current / part
        try:
            current.mkdir(mode=0o700)
        except FileExistsError:
            pass
        if current.is_symlink() or not current.is_dir():
            raise PipelineError(f"{current} is not a plain directory")
        if not current.resolve().is_relative_to(base_resolved):
            raise PipelineError(f"{current} escapes {base_resolved}")
    return current


def _atomic_write(dst: Path, base_resolved: Path, write) -> None:
    parent = dst.parent
    if (
        parent.is_symlink()
        or not parent.is_dir()
        or not parent.resolve().is_relative_to(base_resolved)
    ):
        raise PipelineError(f"refusing to publish into {parent}")
    if dst.is_symlink() or dst.is_dir():
        raise PipelineError(f"refusing to replace {dst}: not a regular file")
    # os.replace swaps the directory entry, so a link planted after the check is replaced rather
    # than written through.
    fd, tmp_name = tempfile.mkstemp(prefix=f".{dst.name}.", dir=parent)
    try:
        with os.fdopen(fd, "wb") as out:
            write(out)
        os.replace(tmp_name, dst)
    except BaseException:
        try:
            os.unlink(tmp_name)
        except FileNotFoundError:
            pass
        raise


def atomic_publish(src: Path, dst: Path, base_resolved: Path) -> None:
    def copy(out) -> None:
        with src.open("rb") as inp:
            shutil.copyfileobj(inp, out)

    _atomic_write(dst, base_resolved, copy)


def atomic_publish_bytes(data: bytes, dst: Path, base_resolved: Path) -> None:
    _atomic_write(dst, base_resolved, lambda out: out.write(data))


def remove_published(paths: list[Path]) -> None:
    for path in paths:
        if path.is_symlink() or path.is_file():
            path.unlink()


def resolve_bin(path: Path | None) -> Path:
    candidates = (path.expanduser(),) if path is not None else DEFAULT_BINS
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    tried = ", ".join(str(c) for c in candidates)
    raise PipelineError(f"qa_record executable not found (tried {tried}); build it first")


def popen(cmd: list[str], **kwargs) -> subprocess.Popen:
    return subprocess.Popen(cmd, stdin=subprocess.DEVNULL, start_new_session=True, **kwargs)


def signal_group(pgid: int, sig: int) -> None:
    try:
        os.killpg(pgid, sig)
    except ProcessLookupError:
        pass


def stop_process(proc: subprocess.Popen) -> None:
    """Stop a child started in its own session together with everything in its process group."""
    leader_done = proc.poll() is not None
    signal_group(proc.pid, signal.SIGTERM)
    if not leader_done:
        try:
            proc.wait(timeout=STOP_TIMEOUT_SECONDS)
        except subprocess.TimeoutExpired:
            pass
    else:
        time.sleep(0.05)
    signal_group(proc.pid, signal.SIGKILL)
    if proc.poll() is None:
        proc.wait()


def run_bounded(cmd: list[str], timeout: float, what: str, **kwargs) -> subprocess.CompletedProcess:
    proc = popen(cmd, **kwargs)
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        stop_process(proc)
        try:
            proc.communicate(timeout=1.0)
        except subprocess.TimeoutExpired:
            for stream in (proc.stdout, proc.stderr):
                if stream is not None:
                    stream.close()
        raise PipelineError(f"{what} timed out after {timeout:g}s") from None
    finally:
        stop_process(proc)
    return subprocess.CompletedProcess(cmd, proc.returncode, out, err)


def run_checked(cmd: list[str], what: str) -> None:
    proc = run_bounded(
        cmd, STEP_TIMEOUT_SECONDS, what, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise PipelineError(f"{what} failed (exit {proc.returncode}):\n{detail}")


def list_scenarios(qa_bin: Path) -> list[str]:
    proc = run_bounded(
        [str(qa_bin), "--list-scenarios"],
        LIST_TIMEOUT_SECONDS,
        "qa_record --list-scenarios",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if proc.returncode != 0:
        raise PipelineError(f"qa_record --list-scenarios failed:\n{proc.stderr.strip()}")
    names = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
    if not names:
        raise PipelineError("qa_record --list-scenarios returned no scenarios")
    bad = [n for n in names if not SCENARIO_NAME.fullmatch(n)]
    if bad:
        raise PipelineError(f"qa_record listed invalid scenario names: {bad!r}")
    return names


def log_tail(path: Path) -> str:
    try:
        lines = path.read_text(errors="replace").splitlines()
    except OSError:
        return ""
    return "\n".join(lines[-LOG_TAIL_LINES:])


def record_timeout(args: argparse.Namespace) -> float:
    return max(MIN_RECORD_TIMEOUT_SECONDS, (args.frames or 0) / 2.0)


def supervise(
    recorder: subprocess.Popen, encoder: subprocess.Popen, timeout: float
) -> tuple[int, int, bool]:
    """Wait for both processes without letting either block forever on the shared FIFO.

    An encoder still waiting to open the FIFO never sees EOF if the recorder dies first, and a
    recorder blocked on open or write never returns if the encoder dies, so the survivor is
    stopped as soon as its peer exits abnormally. Returns (recorder rc, encoder rc, whether the
    encoder failed first); raises PipelineError after reaping both when the deadline passes.
    """
    deadline = time.monotonic() + timeout
    while True:
        rec_rc = recorder.poll()
        enc_rc = encoder.poll()
        if rec_rc is not None and rec_rc != 0:
            stop_process(encoder)
            return rec_rc, encoder.returncode, False
        if enc_rc is not None and enc_rc != 0:
            stop_process(recorder)
            return recorder.returncode, enc_rc, True
        if rec_rc == 0 and enc_rc == 0:
            return 0, 0, False
        if time.monotonic() >= deadline:
            stop_process(recorder)
            stop_process(encoder)
            raise PipelineError(f"recording did not finish within {timeout:g}s")
        time.sleep(POLL_SECONDS)


def recorder_command(qa_bin: Path, name: str, args: argparse.Namespace) -> list[str]:
    cmd = [str(qa_bin), "--scenario", name]
    if args.frames is not None:
        cmd += ["--frames", str(args.frames)]
    if args.no_bgm:
        cmd.append("--no-bgm")
    if args.no_post:
        cmd.append("--no-post")
    if args.no_video:
        cmd.append("--no-render")
    return cmd


def record_audio_only(cmd: list[str], log_path: Path, timeout: float) -> None:
    with log_path.open("w") as log:
        proc = run_bounded(cmd, timeout, "qa_record", stdout=log, stderr=subprocess.STDOUT)
    if proc.returncode != 0:
        raise PipelineError(f"qa_record failed (exit {proc.returncode}):\n{log_tail(log_path)}")


def record_with_video(
    cmd: list[str], work: Path, final_mp4: Path, wav: Path, timeout: float
) -> None:
    fifo = work / "video.rgba"
    os.mkfifo(fifo, 0o600)
    video_only = work / "video_only.mp4"
    rec_log = work / "qa_record.log"
    enc_log = work / "ffmpeg.log"
    encode = [
        "ffmpeg", "-hide_banner", "-nostdin", "-loglevel", "error",
        "-f", "rawvideo", "-pix_fmt", "rgba", "-s", VIDEO_SIZE, "-r", VIDEO_FPS, "-i", str(fifo),
        "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p",
        "-y", str(video_only),
    ]

    # Logs go to files: a full stderr pipe would stall ffmpeg, which in turn stalls the FIFO.
    with enc_log.open("w") as enc_out, rec_log.open("w") as rec_out:
        encoder = popen(encode, stdout=subprocess.DEVNULL, stderr=enc_out)
        recorder = None
        try:
            recorder = popen(
                cmd + ["--video-out", str(fifo)], stdout=rec_out, stderr=subprocess.STDOUT
            )
            rec_rc, enc_rc, encoder_failed_first = supervise(recorder, encoder, timeout)
        finally:
            if recorder is not None:
                stop_process(recorder)
            stop_process(encoder)

    encoder_error = f"ffmpeg encode failed (exit {enc_rc}):\n{log_tail(enc_log)}"
    if encoder_failed_first:
        raise PipelineError(encoder_error)
    if rec_rc != 0:
        raise PipelineError(f"qa_record failed (exit {rec_rc}):\n{log_tail(rec_log)}")
    if enc_rc != 0:
        raise PipelineError(encoder_error)
    run_checked(
        [
            "ffmpeg", "-y", "-loglevel", "error", "-i", str(video_only), "-i", str(wav),
            "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart",
            "-shortest", str(final_mp4),
        ],
        "ffmpeg mux",
    )


def extract_snaps(video: Path, snaps_dir: Path, frames: int) -> None:
    if frames >= SNAP_COUNT * 2:
        run_checked(
            ["bash", str(EXTRACT_SNAPS), str(video), str(snaps_dir), str(SNAP_COUNT)],
            "extract_snaps.sh",
        )
        return
    # Too few frames for evenly spaced duration seeks; the first frame stands in for the sheet.
    snaps_dir.mkdir(mode=0o700, exist_ok=True)
    first = snaps_dir / "snap_01.png"
    run_checked(
        ["ffmpeg", "-y", "-loglevel", "error", "-i", str(video), "-frames:v", "1", str(first)],
        "ffmpeg first-frame snapshot",
    )
    shutil.copyfile(first, snaps_dir / "contact_sheet.png")


def _reject_constant(name: str):
    raise ValueError(f"non-finite number {name}")


def _check_finite_tree(node) -> None:
    if isinstance(node, dict):
        for v in node.values():
            _check_finite_tree(v)
    elif isinstance(node, list):
        for v in node:
            _check_finite_tree(v)
    elif isinstance(node, float):
        if not math.isfinite(node):
            raise ValueError("non-finite float")
    elif type(node) is int:
        try:
            if not math.isfinite(node):
                raise ValueError("integer out of finite float range")
        except OverflowError as exc:
            raise ValueError("integer out of finite float range") from exc


def load_report(path: Path) -> dict:
    try:
        report = json.loads(path.read_text(), parse_constant=_reject_constant)
        _check_finite_tree(report)
    except (OSError, UnicodeDecodeError, ValueError) as exc:
        raise PipelineError(f"malformed report {path.name}: {exc}") from None
    if not isinstance(report, dict):
        raise PipelineError(f"malformed report {path.name}: top level is not an object")
    frames = report.get("frames")
    if type(frames) is not int or frames <= 0 or frames > MAX_FRAMES:
        raise PipelineError(
            f"malformed report {path.name}: frames={frames!r} is not a valid frame count"
        )
    return report


def lookup(report: dict, dotted: str):
    node = report
    for key in dotted.split("."):
        if not isinstance(node, dict) or key not in node:
            return None
        node = node[key]
    return node


def scenario_rules(
    name: str, report: dict, args: argparse.Namespace
) -> list[tuple[str, str, object]]:
    frames = report["frames"]
    video = not args.no_video
    bgm = not args.no_bgm
    rules = [
        ("scenario", "==", name),
        ("frames", "==", args.frames) if args.frames is not None else ("frames", ">", 0),
        ("bgm_enabled", "==", bgm),
        ("video_enabled", "==", video),
    ]
    if video:
        rules += [
            ("post_enabled", "==", not args.no_post),
            ("width", "==", VIDEO_WIDTH),
            ("height", "==", VIDEO_HEIGHT),
            ("fps", "==", VIDEO_FPS_INT),
        ]
    rules += [
        ("determinism.fluid_finite", "==", True),
        ("determinism.audio_finite", "==", True),
        ("audio.clip_ratio", "<", 0.01),
        ("audio.se_peak", ">=", 500),
    ]
    if video:
        rules += [
            ("video.black_frames", "==", 0),
            ("video.white_frames", "==", 0),
            ("video.static_frames", "<", frames * 0.5),
        ]
    if bgm:
        rules += [
            ("audio.bgm_tracks_loaded", ">=", 4),
            ("audio.bgm_peak", ">=", 500),
            ("audio.mix_peak", ">=", 1000),
        ]
    if name == "thunderburst_stress":
        rules.append(("game.thunderbursts", ">=", 10))
        if video:
            rules += [
                ("vfx.thunderburst_max_shake_amp", ">", 0),
                ("vfx.thunderburst_max_shake_amp", "<=", 4.5),
                ("vfx.shake_active_ratio", "<=", 0.45),
                ("vfx.max_callouts_active", ">=", 1),
                ("vfx.max_callouts_active", "<=", 12),
            ]
    elif name == "lobby_brush":
        rules.append(("audio.se_peak", ">=", 800))
        if video:
            rules.append(("vfx.max_shake_amp", "<=", 1.0))
    elif name == "act1_combat":
        rules.append(("game.kills", ">=", 1))
    elif name == "boss_showcase" and video:
        rules.append(("video.static_frames", "<", frames * 0.25))
    return rules


def check_passes(value, op: str, limit) -> bool:
    if isinstance(limit, (bool, str)):
        return type(value) is type(limit) and OPS[op](value, limit)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(value) and OPS[op](value, limit)
    except OverflowError:
        return False


def format_limit(limit) -> str:
    if isinstance(limit, (bool, str)):
        return json.dumps(limit)
    if isinstance(limit, float):
        return f"{limit:g}"
    return str(limit)


def evaluate(name: str, report: dict, args: argparse.Namespace) -> list[dict]:
    results = []
    for path, op, limit in scenario_rules(name, report, args):
        value = lookup(report, path)
        results.append(
            {
                "check": f"{path} {op} {format_limit(limit)}",
                "value": value,
                "ok": check_passes(value, op, limit),
            }
        )
    return results


def summarize_metrics(report: dict) -> dict:
    return {
        "frames": report.get("frames"),
        "final_hash": lookup(report, "determinism.final_hash"),
        "se_peak": lookup(report, "audio.se_peak"),
        "bgm_peak": lookup(report, "audio.bgm_peak"),
        "mix_peak": lookup(report, "audio.mix_peak"),
        "static_frames": lookup(report, "video.static_frames"),
        "max_shake_amp": lookup(report, "vfx.max_shake_amp"),
        "tb_shake_amp": lookup(report, "vfx.thunderburst_max_shake_amp"),
        "thunderbursts": lookup(report, "game.thunderbursts"),
        "kills": lookup(report, "game.kills"),
    }


def publish_scenario(
    work: Path, scen_dir: Path, name: str, args: argparse.Namespace, base: Path
) -> dict:
    published = {"report": str(scen_dir / "report.json"), "video": None, "contact_sheet": None}
    atomic_publish(work / "report.json", scen_dir / "report.json", base)
    if args.no_video or args.keep_wav:
        atomic_publish(work / "audio.wav", scen_dir / "audio.wav", base)
    if args.no_video:
        return published
    atomic_publish(work / f"{name}.mp4", scen_dir / f"{name}.mp4", base)
    snaps_out = ensure_safe_dir(base, (name, "snaps"))
    for snap in sorted((work / "snaps").iterdir()):
        if SNAP_FILE.fullmatch(snap.name) or snap.name == "contact_sheet.png":
            atomic_publish(snap, snaps_out / snap.name, base)
    published["video"] = str(scen_dir / f"{name}.mp4")
    published["contact_sheet"] = str(snaps_out / "contact_sheet.png")
    return published


def clear_previous_outputs(scen_dir: Path, name: str) -> None:
    remove_published([scen_dir / "report.json", scen_dir / f"{name}.mp4", scen_dir / "audio.wav"])
    snaps = scen_dir / "snaps"
    if snaps.is_dir() and not snaps.is_symlink():
        remove_published(
            [
                p
                for p in snaps.iterdir()
                if SNAP_FILE.fullmatch(p.name) or p.name == "contact_sheet.png"
            ]
        )


def run_scenario(name: str, qa_bin: Path, out_dir: Path, args: argparse.Namespace) -> dict:
    result = {
        "scenario": name,
        "passed": False,
        "error": None,
        "report": None,
        "video": None,
        "contact_sheet": None,
        "checks": [],
    }
    started = time.monotonic()
    try:
        if not SCENARIO_NAME.fullmatch(name):
            raise PipelineError(f"invalid scenario name {name!r}")
        base = out_dir.resolve()
        if base != out_dir:
            raise PipelineError(f"output directory moved: {out_dir}")
        with tempfile.TemporaryDirectory(prefix="inkstorm-qa-work-") as tmp:
            work = Path(tmp)
            cmd = recorder_command(qa_bin, name, args)
            cmd += ["--audio-out", str(work / "audio.wav")]
            cmd += ["--report-out", str(work / "report.json")]
            if args.no_video:
                record_audio_only(cmd, work / "qa_record.log", record_timeout(args))
            else:
                record_with_video(
                    cmd, work, work / f"{name}.mp4", work / "audio.wav", record_timeout(args)
                )
            report = load_report(work / "report.json")
            if not args.no_video:
                extract_snaps(work / f"{name}.mp4", work / "snaps", report["frames"])
            with publish_lock(base):
                scen_dir = ensure_safe_dir(base, (name,))
                clear_previous_outputs(scen_dir, name)
                result.update(publish_scenario(work, scen_dir, name, args, base))
    except (PipelineError, OSError) as exc:
        result["error"] = str(exc)
        return result

    result["elapsed_s"] = round(time.monotonic() - started, 2)
    result["metrics"] = summarize_metrics(report)
    result["checks"] = evaluate(name, report, args)
    result["passed"] = all(c["ok"] for c in result["checks"])
    return result


def format_value(value) -> str:
    if value is None:
        return "-"
    if isinstance(value, float):
        return f"{value:.3g}"
    return str(value)


def print_summary(results: list[dict], out_dir: Path) -> None:
    metric_columns = [
        ("frames", "frames", 6),
        ("se_peak", "se_peak", 7),
        ("bgm_peak", "bgm_peak", 8),
        ("mix_peak", "mix_peak", 8),
        ("static", "static_frames", 6),
        ("shake", "max_shake_amp", 5),
        ("tb_shake", "tb_shake_amp", 8),
        ("tbursts", "thunderbursts", 7),
        ("kills", "kills", 5),
    ]
    widths = [20, 6] + [width for _, _, width in metric_columns]
    titles = ["scenario", "result"] + [title for title, _, _ in metric_columns]
    print()
    print("  ".join(t.ljust(w) for t, w in zip(titles, widths)))
    print("  ".join("-" * w for w in widths))
    for res in results:
        metrics = res.get("metrics", {})
        cells = [res["scenario"], "PASS" if res["passed"] else "FAIL"]
        cells += [format_value(metrics.get(key)) for _, key, _ in metric_columns]
        print("  ".join(c.ljust(w) for c, w in zip(cells, widths)))

    for res in results:
        if res["error"]:
            print(f"\n[{res['scenario']}] pipeline error:\n{res['error']}")
        for check in res["checks"]:
            if not check["ok"]:
                got = format_value(check["value"])
                print(f"[{res['scenario']}] FAILED {check['check']} (got {got})")

    print(f"\nOutputs: {out_dir}")
    for res in results:
        if res.get("video"):
            print(f"  {res['video']}")
            print(f"  {res['contact_sheet']}")


def _exit_on_sigterm(signum, frame) -> None:
    raise SystemExit(128 + signal.SIGTERM)


def main(argv: list[str]) -> int:
    signal.signal(signal.SIGTERM, _exit_on_sigterm)
    args = parse_args(argv)
    invalid = [n for n in args.scenario if not SCENARIO_NAME.fullmatch(n)]
    if invalid:
        print(f"qa_run: invalid scenario name(s): {', '.join(map(repr, invalid))}", file=sys.stderr)
        return 2
    try:
        out_dir = prepare_out_dir(args.out, args.allow_in_repo)
        qa_bin = resolve_bin(args.bin)
        available = list_scenarios(qa_bin)
    except (PipelineError, OSError) as exc:
        print(f"qa_run: {exc}", file=sys.stderr)
        return 1

    if args.all:
        names = available
    else:
        unknown = [n for n in args.scenario if n not in available]
        if unknown:
            print(
                f"qa_run: unknown scenario(s): {', '.join(unknown)}; "
                f"available: {', '.join(available)}",
                file=sys.stderr,
            )
            return 2
        names = list(dict.fromkeys(args.scenario))

    results = []
    for name in names:
        print(f"qa_run: recording {name} ...", flush=True)
        results.append(run_scenario(name, qa_bin, out_dir, args))

    passed = all(r["passed"] for r in results)
    summary = {
        "passed": passed,
        "out_dir": str(out_dir),
        "qa_record": str(qa_bin),
        "options": {
            "frames": args.frames,
            "no_bgm": args.no_bgm,
            "no_post": args.no_post,
            "no_video": args.no_video,
        },
        "scenarios": results,
    }
    try:
        data = (json.dumps(summary, indent=2, allow_nan=False) + "\n").encode()
        with publish_lock(out_dir):
            atomic_publish_bytes(data, out_dir / "summary.json", out_dir)
    except (PipelineError, OSError, ValueError) as exc:
        print(f"qa_run: failed to publish summary.json: {exc}", file=sys.stderr)
        return 1
    print_summary(results, out_dir)
    n_passed = sum(r["passed"] for r in results)
    verdict = "PASS" if passed else "FAIL"
    print(f"\nqa_run: {verdict} ({n_passed}/{len(results)} scenarios)")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

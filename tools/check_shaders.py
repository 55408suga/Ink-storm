#!/usr/bin/env python3
"""Validate standalone GLSL files and C-embedded #version shaders via glslangValidator."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

C_EXTS = {".c", ".h"}
SHADER_STAGE_BY_EXT = {
    ".vert": "vert",
    ".vs": "vert",
    ".frag": "frag",
    ".fs": "frag",
    ".comp": "comp",
}

def decode_c_string(raw: str) -> str:
    return (
        raw.replace(r"\\", "\x00")
        .replace(r"\n", "\n")
        .replace(r"\t", "\t")
        .replace(r"\r", "\r")
        .replace(r"\"", '"')
        .replace("\x00", "\\")
    )


def extract_c_string_seqs(text: str) -> list[tuple[int, str]]:
    """Extract concatenated C string literals, treating whitespace and C comments as separators."""
    results: list[tuple[int, str]] = []
    n = len(text)
    i = 0
    line = 1
    current_parts: list[str] = []
    current_line = 0

    def flush() -> None:
        nonlocal current_parts, current_line
        if current_parts:
            results.append((current_line, "".join(current_parts)))
            current_parts = []
            current_line = 0

    while i < n:
        ch = text[i]
        if ch.isspace():
            if ch == "\n":
                line += 1
            i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            i += 2
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                if text[i] == "\n":
                    line += 1
                i += 1
            i = min(n, i + 2)
            continue
        if ch == '"':
            if not current_parts:
                current_line = line
            i += 1
            start = i
            while i < n and text[i] != '"':
                if text[i] == "\\" and i + 1 < n:
                    if text[i + 1] == "\n":
                        line += 1
                    i += 2
                else:
                    if text[i] == "\n":
                        line += 1
                    i += 1
            current_parts.append(decode_c_string(text[start:i]))
            if i < n and text[i] == '"':
                i += 1
            continue
        flush()
        if ch == "'":
            i += 1
            while i < n and text[i] != "'":
                if text[i] == "\\" and i + 1 < n:
                    if text[i + 1] == "\n":
                        line += 1
                    i += 2
                else:
                    if text[i] == "\n":
                        line += 1
                    i += 1
            if i < n and text[i] == "'":
                i += 1
            continue
        i += 1

    flush()
    return results


def guess_stage(source: str) -> str:
    if "gl_Position" in source:
        return "vert"
    if "local_size_x" in source:
        return "comp"
    return "frag"


def validate_glsl(glslang: str, source: str, stage: str, label: str) -> bool:
    suffix = f".{stage}"
    with tempfile.NamedTemporaryFile("w", suffix=suffix, delete=False, encoding="utf-8") as tmp:
        tmp.write(source)
        tmp_path = tmp.name
    try:
        proc = subprocess.run(
            [glslang, "-S", stage, tmp_path],
            capture_output=True,
            text=True,
            check=False,
        )
        if proc.returncode != 0:
            print(f"[FAIL] {label} ({stage})")
            out = (proc.stdout + proc.stderr).strip()
            for line in out.splitlines():
                print(f"  {line.replace(tmp_path, label)}")
            return False
        print(f"[OK]   {label} ({stage})")
        return True
    finally:
        os.unlink(tmp_path)


def collect_files(inputs: list[str]) -> list[Path]:
    files: list[Path] = []
    for raw in inputs:
        p = Path(raw)
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            for child in sorted(p.rglob("*")):
                if child.is_file() and (child.suffix in C_EXTS or child.suffix in SHADER_STAGE_BY_EXT):
                    files.append(child)
        else:
            print(f"[WARN] Path not found: {raw}", file=sys.stderr)
    return files


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("Usage: check_shaders.py <file-or-dir> [...]", file=sys.stderr)
        return 2

    glslang = shutil.which("glslangValidator")
    if not glslang:
        print("ERROR: glslangValidator not found. Install with: sudo apt install glslang-tools", file=sys.stderr)
        return 2

    files = collect_files(argv[1:])
    checked = 0
    failed = 0

    for path in files:
        if path.suffix in SHADER_STAGE_BY_EXT:
            checked += 1
            if not validate_glsl(glslang, path.read_text(encoding="utf-8"), SHADER_STAGE_BY_EXT[path.suffix], str(path)):
                failed += 1
            continue

        text = path.read_text(encoding="utf-8", errors="ignore")
        for idx, (line_no, content) in enumerate(extract_c_string_seqs(text), start=1):
            if not content.lstrip().startswith("#version "):
                continue
            stage = guess_stage(content)
            checked += 1
            if not validate_glsl(glslang, content, stage, f"{path}:{line_no} (shader #{idx})"):
                failed += 1

    print(f"Checked {checked} shader(s), {failed} failed.")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

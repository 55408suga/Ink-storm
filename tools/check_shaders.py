#!/usr/bin/env python3
"""Validate standalone GLSL files and C-embedded #version shaders via glslangValidator."""

from __future__ import annotations

import os
import re
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

# Match one or more adjacent C string literals, e.g. static const char *FS = "..." "...";
STRING_SEQ_RE = re.compile(r'(?:"(?:\\.|[^"\\])*"\s*)+', re.MULTILINE)
SINGLE_STR_RE = re.compile(r'"((?:\\.|[^"\\])*)"')


def decode_c_string_seq(raw: str) -> str:
    parts: list[str] = []
    for m in SINGLE_STR_RE.finditer(raw):
        s = m.group(1)
        s = (
            s.replace(r"\\", "\x00")
            .replace(r"\n", "\n")
            .replace(r"\t", "\t")
            .replace(r"\r", "\r")
            .replace(r"\"", '"')
            .replace("\x00", "\\")
        )
        parts.append(s)
    return "".join(parts)


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
        for idx, match in enumerate(STRING_SEQ_RE.finditer(text), start=1):
            content = decode_c_string_seq(match.group(0))
            if not content.lstrip().startswith("#version "):
                continue
            line_no = text.count("\n", 0, match.start()) + 1
            stage = guess_stage(content)
            checked += 1
            if not validate_glsl(glslang, content, stage, f"{path}:{line_no} (shader #{idx})"):
                failed += 1

    print(f"Checked {checked} shader(s), {failed} failed.")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

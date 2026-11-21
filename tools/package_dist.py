#!/usr/bin/env python3
"""Build and verify the standalone source distribution archive for Inkstorm."""

import os
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath
from zipfile import ZIP_DEFLATED, BadZipFile, ZipFile

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "dist" / "inkstorm-src.zip"
DIST_THIRD_PARTY = []

FORBIDDEN_PARTS = {
    "AGENTS.md",
    "CLAUDE.md",
    "docs",
    "tests",
    "tools",
    ".github",
    ".coderabbit.yaml",
    ".agents",
    ".claude",
    "flake.nix",
    "flake.lock",
    ".envrc",
    "CMakeLists.txt",
    "CMakePresets.json",
}


def tree_files(directory):
    return sorted(
        path for path in directory.rglob("*") if not path.is_symlink() and path.is_file()
    )


def write_archive(archive_path):
    sources = [path for path in tree_files(ROOT / "src") if path.suffix in {".c", ".h"}]
    assets = ROOT / "assets"
    asset_files = [path for path in tree_files(assets) if path.name != ".gitkeep"]
    dependencies = [
        path
        for library in DIST_THIRD_PARTY
        for path in tree_files(ROOT / "third_party" / library)
    ]

    with ZipFile(archive_path, "w", compression=ZIP_DEFLATED) as archive:
        archive.write(assets, "assets/")
        for path in sources + asset_files + dependencies:
            archive.write(path, path.relative_to(ROOT).as_posix())
        archive.write(ROOT / "tools" / "dist.mk", "Makefile")
        archive.write(ROOT / "README.txt", "README.txt")


def verify_archive(archive_path, build_dir, raylib_dir):
    with ZipFile(archive_path) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate archive entries")
        for name in names:
            path = PurePosixPath(name)
            if (
                path.is_absolute()
                or ".." in path.parts
                or FORBIDDEN_PARTS.intersection(path.parts)
                or path.parts[:2] == ("third_party", "unity")
            ):
                raise ValueError(f"Forbidden archive entry: {name}")
        print(f"Archive check: {len(names)} entries, no forbidden paths.", flush=True)
        archive.extractall(build_dir)

    host_env = {"PATH": "/usr/bin:/bin", "HOME": str(Path.home())}
    subprocess.run(["make", f"RAYLIB_DIR={raylib_dir}"], cwd=build_dir, env=host_env, check=True)
    subprocess.run(["./inkstorm", "--headless-smoke"], cwd=build_dir, env=host_env, check=True)


def main():
    default_raylib = Path.home() / ".local" / "opt" / "raylib-6.2"
    raylib_dir = Path(os.environ.get("RAYLIB_DIR") or default_raylib).expanduser().resolve()
    try:
        OUTPUT.parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".inkstorm-", dir=OUTPUT.parent) as temp:
            work_dir = Path(temp)
            archive_path = work_dir / OUTPUT.name
            write_archive(archive_path)
            verify_archive(archive_path, work_dir / "check", raylib_dir)
            archive_path.replace(OUTPUT)
    except (OSError, ValueError, BadZipFile, subprocess.CalledProcessError) as error:
        print(f"Distribution verification failed: {error}", file=sys.stderr)
        return 1

    print(f"Verified source archive: {OUTPUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

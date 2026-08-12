#!/usr/bin/env python3
"""Format every C++ source and header in this repository with clang-format 17."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parent
SOURCE_SUFFIXES = {".cpp", ".h"}
IGNORED_DIRECTORIES = {".git", "build"}
REQUIRED_MAJOR_VERSION = 17


def executable_candidates() -> list[str]:
    """Return platform-appropriate clang-format candidate paths in preference order."""
    candidates = ["clang-format-17", "clang-format"]

    if sys.platform == "darwin":
        candidates[:0] = [
            "/opt/homebrew/opt/llvm@17/bin/clang-format",
            "/usr/local/opt/llvm@17/bin/clang-format",
        ]
        try:
            prefix = subprocess.check_output(
                ["brew", "--prefix", "llvm@17"], text=True, stderr=subprocess.DEVNULL
            ).strip()
            candidates.insert(0, str(Path(prefix) / "bin" / "clang-format"))
        except (FileNotFoundError, subprocess.CalledProcessError):
            pass

    return candidates


def clang_format_version(executable: str) -> int | None:
    try:
        output = subprocess.check_output([executable, "--version"], text=True)
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None

    match = re.search(r"(?:clang-format|LLVM)(?: version)? (\d+)", output)
    return int(match.group(1)) if match else None


def find_clang_format() -> str:
    for candidate in executable_candidates():
        executable = candidate if os.path.sep in candidate else shutil.which(candidate)
        if not executable:
            continue
        version = clang_format_version(executable)
        if version == REQUIRED_MAJOR_VERSION:
            return executable
        if version is not None:
            print(
                f"Ignoring {executable}: found clang-format {version}, "
                f"but version {REQUIRED_MAJOR_VERSION} is required.",
                file=sys.stderr,
            )

    raise RuntimeError(
        "clang-format 17 was not found. Install LLVM 17 and make clang-format-17 available on PATH."
    )


def source_files() -> list[Path]:
    return sorted(
        path
        for path in REPOSITORY_ROOT.rglob("*")
        if path.is_file()
        and path.suffix in SOURCE_SUFFIXES
        and not any(directory in IGNORED_DIRECTORIES for directory in path.relative_to(REPOSITORY_ROOT).parts)
    )


def main() -> int:
    try:
        clang_format = find_clang_format()
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1

    files = source_files()
    if not files:
        print("No .cpp or .h files found.")
        return 0

    print(f"Formatting {len(files)} files with {clang_format}.")
    subprocess.run([clang_format, "-i", *(str(path) for path in files)], check=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from error

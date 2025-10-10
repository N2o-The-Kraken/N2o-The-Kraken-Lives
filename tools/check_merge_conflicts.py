#!/usr/bin/env python3
"""Utility to detect unresolved merge conflicts in the repository."""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import sys
from typing import Iterable, List

CONFLICT_PATTERNS: tuple[re.Pattern[str], ...] = (
    re.compile(r"^<{7}( .*)?$", re.MULTILINE),
    re.compile(r"^={7}$", re.MULTILINE),
    re.compile(r"^>{7}( .*)?$", re.MULTILINE),
)
SKIP_DIRS: tuple[str, ...] = (".git", "build", "cmake-build-debug", "cmake-build-release")


def iter_files(root: pathlib.Path) -> Iterable[pathlib.Path]:
    for current_root, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in SKIP_DIRS]
        current_path = pathlib.Path(current_root)
        for filename in filenames:
            yield current_path / filename


def file_has_conflict_markers(path: pathlib.Path) -> bool:
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return False
    return any(pattern.search(text) for pattern in CONFLICT_PATTERNS)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root",
        nargs="?",
        default=pathlib.Path(__file__).resolve().parents[1],
        type=pathlib.Path,
        help="Repository root to scan (defaults to repository root).",
    )
    args = parser.parse_args(argv)

    root = args.root.resolve()
    offending: list[pathlib.Path] = []

    for path in iter_files(root):
        if file_has_conflict_markers(path):
            offending.append(path)

    if offending:
        for path in offending:
            print(path, file=sys.stderr)
        print(
            f"Found {len(offending)} file(s) containing merge conflict markers.",
            file=sys.stderr,
        )
        return 1

    print("No merge conflict markers detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

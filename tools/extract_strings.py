#!/usr/bin/env python3
"""Extract filtered ASCII and UTF-16LE strings from a binary.

The script is intentionally dependency-free so reconnaissance can be repeated on
an untouched game executable. It prints offsets only; it never writes beside the
input file.
"""

from __future__ import annotations

import argparse
import mmap
import re
from collections.abc import Iterator
from pathlib import Path


def iter_ascii(data: mmap.mmap, minimum: int) -> Iterator[tuple[int, str, str]]:
    pattern = re.compile(rb"[\x20-\x7e]{%d,}" % minimum)
    for match in pattern.finditer(data):
        yield match.start(), "ascii", match.group().decode("ascii")


def iter_utf16le(data: mmap.mmap, minimum: int) -> Iterator[tuple[int, str, str]]:
    pattern = re.compile(rb"(?:[\x20-\x7e]\x00){%d,}" % minimum)
    for match in pattern.finditer(data):
        yield match.start(), "utf16le", match.group().decode("utf-16le")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument(
        "patterns",
        nargs="*",
        help="case-insensitive regular expressions; all strings are shown if omitted",
    )
    parser.add_argument("--minimum", type=int, default=5)
    parser.add_argument(
        "--start",
        type=lambda value: int(value, 0),
        default=0,
        help="first file offset to include (decimal or 0x-prefixed)",
    )
    parser.add_argument(
        "--end",
        type=lambda value: int(value, 0),
        help="exclusive final file offset to include (decimal or 0x-prefixed)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.minimum < 2:
        raise SystemExit("--minimum must be at least 2")

    filters = [re.compile(pattern, re.IGNORECASE) for pattern in args.patterns]
    with args.binary.open("rb") as binary_file:
        with mmap.mmap(binary_file.fileno(), 0, access=mmap.ACCESS_READ) as data:
            strings = list(iter_ascii(data, args.minimum))
            strings.extend(iter_utf16le(data, args.minimum))

    for offset, encoding, value in sorted(strings):
        if offset < args.start or (args.end is not None and offset >= args.end):
            continue
        if not filters or any(pattern.search(value) for pattern in filters):
            try:
                print(f"0x{offset:08X}\t{encoding}\t{value}")
            except BrokenPipeError:
                return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

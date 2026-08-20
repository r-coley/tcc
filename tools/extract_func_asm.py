#!/usr/bin/env python3
from __future__ import annotations

import argparse

from macho_disasm import existing_file, load_functions, positive_int


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Extract one named function's disassembly from a Mach-O binary."
    )
    parser.add_argument("binary", type=existing_file)
    parser.add_argument("function", help="Exact symbol name, e.g. _sqlite3WalCallback")
    parser.add_argument("--arch", default=None, help="Pass through to otool -arch")
    parser.add_argument(
        "--index",
        type=positive_int,
        default=1,
        help="1-based occurrence index if the symbol appears more than once",
    )
    parser.add_argument(
        "--count-only",
        action="store_true",
        help="Print only the instruction count for the selected function",
    )
    args = parser.parse_args()

    matches = [block for block in load_functions(args.binary, arch=args.arch) if block.name == args.function]
    if args.index > len(matches):
        raise SystemExit(
            f"function {args.function!r} occurrence {args.index} not found in {args.binary}"
        )
    block = matches[args.index - 1]
    if args.count_only:
        print(block.instruction_count)
        return
    for line in block.lines:
        print(line)


if __name__ == "__main__":
    main()

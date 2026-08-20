#!/usr/bin/env python3
from __future__ import annotations

import argparse
from typing import Any

from macho_disasm import existing_file, group_by_name, load_functions


def format_count(value: int | None) -> str:
    return "-" if value is None else str(value)


def row_name(name: str, index: int, total: int) -> str:
    if total <= 1:
        return name
    return f"{name}#{index + 1}"


def sort_key(kind: str, row: tuple[str, int | None, int | None, float | None]) -> Any:
    name, clang_count, tcc_count, _ = row
    if kind == "name":
        return name
    if kind == "delta":
        lhs = -1 if clang_count is None else clang_count
        rhs = -1 if tcc_count is None else tcc_count
        return (abs(rhs - lhs), name)
    if kind == "tcc":
        return (10**9 if tcc_count is None else tcc_count, name)
    return (10**9 if clang_count is None else clang_count, name)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare Mach-O function instruction counts between two binaries."
    )
    parser.add_argument("clang_binary", type=existing_file)
    parser.add_argument("tcc_binary", type=existing_file)
    parser.add_argument("--arch", default=None, help="Pass through to otool -arch")
    parser.add_argument(
        "--sort",
        choices=("clang", "tcc", "delta", "name"),
        default="clang",
        help="Sort rows by clang count, tcc count, absolute delta, or name",
    )
    parser.add_argument(
        "--descending",
        action="store_true",
        help="Reverse the selected sort order",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Include functions that exist in only one binary",
    )
    parser.add_argument(
        "--only-different",
        action="store_true",
        help="Show only rows where the counts differ",
    )
    parser.add_argument(
        "--only-tcc-larger",
        action="store_true",
        help="Show only rows where the tcc instruction count is larger than clang",
    )
    parser.add_argument(
        "--only-clang-larger",
        action="store_true",
        help="Show only rows where the clang instruction count is larger than tcc",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Limit the number of printed rows",
    )
    parser.add_argument(
        "--min-percent-over",
        type=float,
        default=None,
        help="Show only rows where tcc is at least this many percent larger than clang",
    )
    parser.add_argument(
        "--show-percent",
        action="store_true",
        help="Show the percentage by which tcc differs from clang",
    )
    args = parser.parse_args()

    if args.only_tcc_larger and args.only_clang_larger:
        parser.error("--only-tcc-larger and --only-clang-larger are mutually exclusive")
    if args.min_percent_over is not None and args.min_percent_over < 0:
        parser.error("--min-percent-over must be >= 0")

    clang = group_by_name(load_functions(args.clang_binary, arch=args.arch))
    tcc = group_by_name(load_functions(args.tcc_binary, arch=args.arch))

    if args.all:
        names = sorted(set(clang) | set(tcc))
    else:
        names = sorted(set(clang) & set(tcc))

    rows: list[tuple[str, int | None, int | None, float | None]] = []
    for name in names:
        clang_blocks = clang.get(name, [])
        tcc_blocks = tcc.get(name, [])
        total = max(len(clang_blocks), len(tcc_blocks))
        for idx in range(total):
            clang_count = clang_blocks[idx].instruction_count if idx < len(clang_blocks) else None
            tcc_count = tcc_blocks[idx].instruction_count if idx < len(tcc_blocks) else None
            percent_over = None
            if args.only_different and clang_count == tcc_count:
                continue
            if args.only_tcc_larger:
                if clang_count is None or tcc_count is None or tcc_count <= clang_count:
                    continue
            if args.only_clang_larger:
                if clang_count is None or tcc_count is None or clang_count <= tcc_count:
                    continue
            if args.min_percent_over is not None:
                if clang_count is None or tcc_count is None or clang_count <= 0:
                    continue
                percent_over = ((tcc_count - clang_count) * 100.0) / clang_count
                if percent_over < args.min_percent_over:
                    continue
            if percent_over is None and clang_count is not None and tcc_count is not None and clang_count > 0:
                percent_over = ((tcc_count - clang_count) * 100.0) / clang_count
            rows.append((row_name(name, idx, total), clang_count, tcc_count, percent_over))

    rows.sort(key=lambda row: sort_key(args.sort, row), reverse=args.descending)
    if args.limit > 0:
        rows = rows[: args.limit]

    name_width = max([len("function")] + [len(name) for name, _, _, _ in rows])
    clang_width = max(len("clang"), max((len(format_count(v)) for _, v, _, _ in rows), default=0))
    tcc_width = max(len("tcc"), max((len(format_count(v)) for _, _, v, _ in rows), default=0))
    pct_width = max(len("%over"), max((len(f"{v:.1f}") for _, _, _, v in rows if v is not None), default=0))

    if args.show_percent:
        print(
            f"{'function':<{name_width}} {'clang':>{clang_width}} {'tcc':>{tcc_width}} {'%over':>{pct_width}}"
        )
        for name, clang_count, tcc_count, percent_over in rows:
            pct_text = "-" if percent_over is None else f"{percent_over:.1f}"
            print(
                f"{name:<{name_width}} "
                f"{format_count(clang_count):>{clang_width}} "
                f"{format_count(tcc_count):>{tcc_width}} "
                f"{pct_text:>{pct_width}}"
            )
    else:
        print(f"{'function':<{name_width}} {'clang':>{clang_width}} {'tcc':>{tcc_width}}")
        for name, clang_count, tcc_count, _ in rows:
            print(
                f"{name:<{name_width}} "
                f"{format_count(clang_count):>{clang_width}} "
                f"{format_count(tcc_count):>{tcc_width}}"
            )


if __name__ == "__main__":
    main()

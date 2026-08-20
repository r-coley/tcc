#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


FUNCTION_RE = re.compile(r"^([_A-Za-z.$][\w.$]*):$")
INSTRUCTION_RE = re.compile(r"^[0-9A-Fa-f]{8,16}\t")


@dataclass
class FunctionBlock:
    name: str
    lines: list[str]
    instruction_count: int


def run_otool(binary: str, arch: str | None = None) -> list[str]:
    cmd = ["otool"]
    if arch:
        cmd.extend(["-arch", arch])
    cmd.extend(["-tvV", binary])
    try:
        proc = subprocess.run(cmd, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr or exc.stdout)
        raise SystemExit(exc.returncode) from exc
    return proc.stdout.splitlines()


def parse_functions(lines: Iterable[str]) -> list[FunctionBlock]:
    blocks: list[FunctionBlock] = []
    current_name: str | None = None
    current_lines: list[str] = []
    current_count = 0

    def flush() -> None:
        nonlocal current_name, current_lines, current_count
        if current_name is not None:
            blocks.append(FunctionBlock(current_name, current_lines[:], current_count))
        current_name = None
        current_lines = []
        current_count = 0

    for raw_line in lines:
        line = raw_line.rstrip("\n")
        match = FUNCTION_RE.match(line)
        if match and not line.startswith("/"):
            flush()
            current_name = match.group(1)
            current_lines = [line]
            current_count = 0
            continue
        if current_name is None:
            continue
        current_lines.append(line)
        if INSTRUCTION_RE.match(line):
            current_count += 1

    flush()
    return blocks


def load_functions(binary: str, arch: str | None = None) -> list[FunctionBlock]:
    return parse_functions(run_otool(binary, arch=arch))


def group_by_name(blocks: Iterable[FunctionBlock]) -> dict[str, list[FunctionBlock]]:
    grouped: dict[str, list[FunctionBlock]] = {}
    for block in blocks:
        grouped.setdefault(block.name, []).append(block)
    return grouped


def positive_int(value: str) -> int:
    ivalue = int(value, 10)
    if ivalue <= 0:
        raise argparse.ArgumentTypeError("must be > 0")
    return ivalue


def existing_file(value: str) -> str:
    path = Path(value)
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"file not found: {value}")
    return str(path)


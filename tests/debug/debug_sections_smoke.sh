#!/bin/sh

CC=
TMP=
TEST_DIR=tests

while [ $# -gt 0 ]; do
    case "$1" in
        -c) CC=$2; shift 2 ;;
        -T) TMP=$2; shift 2 ;;
        -d) TEST_DIR=$2; shift 2 ;;
        --flags) shift 2 ;;
        *)
            echo "unknown option: $1" >&2
            exit 2
            ;;
    esac
done

if [ -z "$CC" ] || [ -z "$TMP" ]; then
    echo "usage: $0 -c <compiler> -T <tmp-dir> [-d tests-dir]" >&2
    exit 2
fi

mkdir -p "$TMP"
fail=0

check_target() {
    target=$1
    src="$TEST_DIR/core/test001.c"
    no_g="$TMP/debug_sections_${target}_no_g.s"
    with_g="$TMP/debug_sections_${target}_with_g.s"

    if "$CC" -S -target="$target" "$src" -o "$no_g" >/dev/null 2>"$no_g.err" &&
       "$CC" -S -target="$target" -g "$src" -o "$with_g" >/dev/null 2>"$with_g.err"; then
        echo "  PASS compile $target"
    else
        echo "  FAIL compile $target"
        cat "$no_g.err" "$with_g.err"
        fail=$((fail + 1))
        return
    fi

    if grep -Eq 'DWARF|debug_' "$no_g"; then
        echo "  FAIL $target no -g emits debug sections"
        fail=$((fail + 1))
    else
        echo "  CHECK $target no -g has no debug sections"
    fi

    if grep -Eq 'DWARF|debug_' "$with_g"; then
        echo "  CHECK $target -g emits debug sections"
    else
        echo "  FAIL $target -g missing debug sections"
        fail=$((fail + 1))
    fi
}

echo "debug section emission smoke test:"

check_target arm64
check_target x64

[ "$fail" -eq 0 ] || exit 1
echo "debug section smoke test OK"

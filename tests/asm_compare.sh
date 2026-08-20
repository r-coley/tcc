#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
LIST_FILE=${1:-"$ROOT_DIR/tests/asm-quality.txt"}
OUT_DIR=${ASM_COMPARE_OUT:-"$ROOT_DIR/build/asm-compare"}
TARGET=${TEST_TARGET:-arm64}
BOOT_FLAGS=${BOOT_FLAGS:--boot}
CLANG_BIN=${CLANG_BIN:-clang}
TCC_STAGE0=${TCC_STAGE0:-"$ROOT_DIR/build/tcc_stage0"}
TCC_STAGE1=${TCC_STAGE1:-"$ROOT_DIR/build/tcc_stage1"}

mkdir -p "$OUT_DIR"

clang_target_flags() {
    case "$TARGET" in
        arm64)
            printf '%s\n' "-target arm64-apple-macos11"
            ;;
        x64)
            printf '%s\n' "-target x86_64-apple-macos11"
            ;;
        *)
            printf '%s\n' ""
            ;;
    esac
}

count_labels() {
    awk '/^[[:space:]]*[A-Za-z_.$][A-Za-z0-9_.$]*:([[:space:]]*(;.*)?)?$/ { count++ } END { print count + 0 }' "$1"
}

count_instructions() {
    awk '
        /^[[:space:]]*[A-Za-z_.$][A-Za-z0-9_.$]*:/ { next }
        /^[[:space:]]*\./ { next }
        /^[[:space:]]*$/ { next }
        /^[[:space:]]*;/ { next }
        /^[[:space:]]*[A-Za-z][A-Za-z0-9_.]*/ { count++ }
        END { print count + 0 }
    ' "$1"
}

count_stack_refs() {
    case "$TARGET" in
        arm64)
            grep -E -c '\[(sp|x29)' "$1" || true
            ;;
        x64)
            grep -E -c '(%rsp|%rbp)' "$1" || true
            ;;
        *)
            printf '0\n'
            ;;
    esac
}

count_calls() {
    case "$TARGET" in
        arm64)
            grep -E -c '(^|[[:space:]])bl([[:space:]]|$)' "$1" || true
            ;;
        x64)
            grep -E -c '(^|[[:space:]])callq?([[:space:]]|$)' "$1" || true
            ;;
        *)
            printf '0\n'
            ;;
    esac
}

print_metrics_row() {
    compiler_name=$1
    asm_file=$2
    line_count=$(wc -l < "$asm_file" | tr -d ' ')
    byte_count=$(wc -c < "$asm_file" | tr -d ' ')
    label_count=$(count_labels "$asm_file")
    inst_count=$(count_instructions "$asm_file")
    stack_refs=$(count_stack_refs "$asm_file")
    call_count=$(count_calls "$asm_file")

    printf "%-12s %8s %9s %8s %8s %10s %8s\n" \
        "$compiler_name" "$line_count" "$byte_count" "$label_count" \
        "$inst_count" "$stack_refs" "$call_count"
}

compile_with_clang() {
    src=$1
    out=$2
    target_flags=$(clang_target_flags)

    if [ -n "$target_flags" ]; then
        # shellcheck disable=SC2086
        "$CLANG_BIN" $target_flags -S -O0 \
            -fno-asynchronous-unwind-tables \
            -fno-unwind-tables \
            -fno-stack-protector \
            -o "$out" "$src"
    else
        "$CLANG_BIN" -S -O0 \
            -fno-asynchronous-unwind-tables \
            -fno-unwind-tables \
            -fno-stack-protector \
            -o "$out" "$src"
    fi
}

compile_with_tcc() {
    compiler=$1
    src=$2
    out=$3

    "$compiler" $BOOT_FLAGS -target="$TARGET" -S "$src" -o "$out"
}

printf "Assembly comparison target=%s\n" "$TARGET"
printf "test list: %s\n" "$LIST_FILE"
printf "output:    %s\n" "$OUT_DIR"
printf "\n"

while IFS= read -r rel_path; do
    case "$rel_path" in
        ''|\#*)
            continue
            ;;
    esac

    src_path=$ROOT_DIR/$rel_path
    case_dir=$(dirname "$rel_path")
    base_name=$(basename "$rel_path" .c)
    out_base=$OUT_DIR/$(printf '%s' "$case_dir" | tr '/' '_')__$base_name

    clang_out=$out_base.clang.s
    tcc0_out=$out_base.tcc.s
    tcc1_out=$out_base.tcc_stage1.s

    compile_with_clang "$src_path" "$clang_out"
    compile_with_tcc "$TCC_STAGE0" "$src_path" "$tcc0_out"
    compile_with_tcc "$TCC_STAGE1" "$src_path" "$tcc1_out"

    printf "%s\n" "$rel_path"
    printf "%-12s %8s %9s %8s %8s %10s %8s\n" \
        "compiler" "lines" "bytes" "labels" "instrs" "stackrefs" "calls"
    print_metrics_row "clang" "$clang_out"
    print_metrics_row "tcc" "$tcc0_out"
    print_metrics_row "tcc_stage1" "$tcc1_out"
    printf "\n"
done < "$LIST_FILE"

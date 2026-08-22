#!/bin/sh
# runtests.sh — Unified TCC test harness
#
# Usage:
#   tests/:runtests.sh [options]
#
# Options:
#   -c COMPILER   Path to compiler (default: build/tcc_stage0)
#   -t TARGET     Compilation target: arm64 | x64 | x86 | mips
#                 (default: auto-detected from uname -m)
#   -a ASSEMBLER  Assembler/linker to use (default: clang)
#   -C CATEGORY   Run only tests whose path starts with CATEGORY/
#   -m MANIFEST   Path to manifest file (default: tests/manifest.txt)
#   -D TMPDIR     Scratch directory for compiled output (default: build/tmp)
#   --type TYPE   Run only tests of this type (run|linkerror|error|warn|nowarn|dwarf|todo)
#   --filter TEXT Run only tests whose manifest path contains TEXT
#   -v            Verbose: print each test name and outcome as it runs
#   -h            Show this help
#
# Manifest format:  type:path:flags:expected
#
#   run       compile + assemble + run; expected = exit code
#   linkerror compile + link, expect failure; expected = stderr substring
#   error     compile only, expect failure; expected = stderr substring
#   warn      compile, check stderr contains substring; expected = substring
#   nowarn    compile with flags, check stderr is empty; expected = (empty)
#   dwarf     compile -S, check asm for patterns; expected = space-separated patterns
#   dwarfverify compile -c, run dwarfdump --verify on the object
#   todo      like run but non-gating (prints SKIP on failure, never fails)
#
# For run/todo/linkerror tests, flags may include:
#   extra=path1.c,path2.c         compile extra sources with tcc in the same step
#   clangextra=path1.c,path2.c    compile extra sources with clang and link them
#                                 with the tcc-built primary source
#   stdout=path.expected           compare program stdout with tests/path.expected
#
# Exit code: number of failed tests (0 = all passed).

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
COMPILER="build/tcc_stage0"
ASSEMBLER="clang"
CATEGORY=""
MANIFEST="tests/manifest.txt"
TMPDIR="build/tmp"
TYPE_FILTER=""
NAME_FILTER=""
VERBOSE=0
TCC_TEST_FLAGS=${TCC_TEST_FLAGS:-}

case "$(uname -m 2>/dev/null)" in
    arm64|aarch64)        DEFAULT_TARGET="arm64" ;;
    x86_64|amd64)         DEFAULT_TARGET="x64"   ;;
    i386|i486|i586|i686)  DEFAULT_TARGET="x86"   ;;
    *)                    DEFAULT_TARGET=""       ;;
esac
TARGET="$DEFAULT_TARGET"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        -c) COMPILER="$2";      shift 2 ;;
        -t) TARGET="$2";        shift 2 ;;
        -a) ASSEMBLER="$2";     shift 2 ;;
        -C) CATEGORY="$2";      shift 2 ;;
        -m) MANIFEST="$2";      shift 2 ;;
        -D) TMPDIR="$2";        shift 2 ;;
        --type) TYPE_FILTER="$2"; shift 2 ;;
        --filter) NAME_FILTER="$2"; shift 2 ;;
        -v) VERBOSE=1;          shift ;;
        -h)
            sed -n '2,/^[^#]/{ /^#/{ s/^# \{0,1\}//; p }; /^[^#]/q }' "$0"
            exit 0
            ;;
        --) shift; break ;;
        -*) echo "runtests.sh: unknown option $1" >&2; exit 1 ;;
        *)  break ;;
    esac
done

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
if [ -z "$TARGET" ]; then
    echo "runtests.sh: cannot auto-detect target; use -t arm64|x64|x86|mips" >&2
    exit 1
fi

if [ ! -f "$MANIFEST" ]; then
    echo "runtests.sh: manifest not found: $MANIFEST" >&2
    exit 1
fi

if [ ! -x "$COMPILER" ]; then
    echo "runtests.sh: compiler not found or not executable: $COMPILER" >&2
    exit 1
fi

mkdir -p "$TMPDIR"

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------
fail=0
pass=0
skip=0
tests=0
last_newline=1
current_class=""

# asm cache: maps "path|flags" -> assembled .s file path, avoids recompiling
# the same source for multiple dwarf pattern checks.
RUN_ID=$$
ASM_CACHE="$TMPDIR/_asm_cache_${RUN_ID}.txt"
: > "$ASM_CACHE"

asm_key() { printf '%s|%s' "$1" "$2" | tr ' /=' '___'; }

asm_cache_get() {
    k=$(asm_key "$1" "$2")
    grep "^${k}=" "$ASM_CACHE" 2>/dev/null | head -1 | cut -d= -f2-
}

asm_cache_set() {
    k=$(asm_key "$1" "$2")
    printf '%s=%s\n' "$k" "$3" >> "$ASM_CACHE"
}

# ---------------------------------------------------------------------------
# Header
# ---------------------------------------------------------------------------
echo "Running tests:"
echo "  compiler:  $COMPILER"
echo "  target:    $TARGET"
echo "  assembler: $ASSEMBLER"
[ -n "$TCC_TEST_FLAGS" ] && echo "  flags:     $TCC_TEST_FLAGS"
[ -n "$CATEGORY" ]       && echo "  category:  $CATEGORY"
[ -n "$TYPE_FILTER" ]    && echo "  type:      $TYPE_FILTER"
[ -n "$NAME_FILTER" ]    && echo "  filter:    $NAME_FILTER"
echo ""

verbose_status() {
    [ "$VERBOSE" -eq 1 ] || return 1
    [ "$last_newline" -eq 0 ] && echo
    printf "  %s %s [%s]\n" "$1" "$base" "$type"
    last_newline=1
    return 0
}

verbose_cmd() {
    [ "$VERBOSE" -eq 1 ] || return 0
    printf "    + %s\n" "$1"
}

clang_target_flag() {
    case "$(uname -s 2>/dev/null)" in
        Darwin)
            case "$TARGET" in
                arm64) printf '%s\n' "arm64-apple-macos" ;;
                x64|x86_64) printf '%s\n' "x86_64-apple-macos" ;;
                *) printf '%s\n' "" ;;
            esac
            ;;
        *)
            printf '%s\n' ""
            ;;
    esac
}

# ---------------------------------------------------------------------------
# Manifest ordering
# ---------------------------------------------------------------------------
# The manifest is allowed to grow by appending tests.  Sort active entries at
# runtime so output remains grouped by test category even if matching paths are
# not contiguous in tests/manifest.txt.  Comments and blank lines are ignored.
SORTED_MANIFEST="$TMPDIR/_manifest_${RUN_ID}.sorted"
grep -v '^[[:space:]]*$' "$MANIFEST" | \
    grep -v '^[[:space:]]*#' | \
    LC_ALL=C sort -t: -k2,2 -k1,1 -k3,3 > "$SORTED_MANIFEST"

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
while IFS= read -r entry; do
    case "$entry" in ''|\#*) continue ;; esac

    # Parse type:path:flags:expected[:desc]  (desc is optional)
    type="${entry%%:*}"
    rest="${entry#*:}"
    path="${rest%%:*}"
    rest="${rest#*:}"
    flags="${rest%%:*}"
    rest="${rest#*:}"
    expected="${rest%%:*}"
    desc="${rest#*:}"
    [ "$desc" = "$expected" ] && desc=""   # no 5th field

    # Filters
    [ -n "$TYPE_FILTER" ] && [ "$type" != "$TYPE_FILTER" ] && continue
    if [ -n "$NAME_FILTER" ]; then
        case "$path" in *"$NAME_FILTER"*) ;; *) continue ;; esac
    fi
    if [ -n "$CATEGORY" ]; then
        case "$path" in "$CATEGORY"/*) ;; *) continue ;; esac
    fi

    tests=$((tests + 1))
    src="tests/$path"
    base="${path%.c}"
    class="${path%%/*}"
    safename=$(printf '%s' "$base" | tr '/. ' '___')

    # Category header
    if [ "$class" != "$current_class" ]; then
        [ "$last_newline" -eq 0 ] && echo
        current_class="$class"
        printf "%s: " "$current_class"
        last_newline=0
    fi

    [ "$VERBOSE" -eq 1 ] && {
        [ "$last_newline" -eq 0 ] && echo
        printf "  RUN  %s [%s]\n" "$base" "$type"
        last_newline=1
    }

    # -------------------------------------------------------------------
    case "$type" in

    run|todo|linkerror)
        asm="$TMPDIR/${safename}.s"
        exe="$TMPDIR/${safename}"
        err="$TMPDIR/${safename}.err"
        clang_target=$(clang_target_flag)

        extra_src=""
        clang_extra_src=""
        expected_stdout=""
        clean_flags=""
        for f in $flags; do
            case "$f" in
                extra=*) extra_src="${f#extra=}" ;;
                clangextra=*) clang_extra_src="${f#clangextra=}" ;;
                stdout=*) expected_stdout="${f#stdout=}" ;;
                *) clean_flags="$clean_flags $f" ;;
            esac
        done

        ok=1
        if [ -n "$clang_extra_src" ] && [ -n "$extra_src" ]; then
            printf "runtests.sh: %s cannot combine extra= with clangextra=\n" "$base" >&2
            exit 1
        fi

        if [ -n "$clang_extra_src" ]; then
            obj="$TMPDIR/${safename}.o"
            link_inputs="$obj"
            clang_target=$(clang_target_flag)

            verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$TARGET $clean_flags -S $src -o $asm"
            # shellcheck disable=SC2086
            "$COMPILER" $TCC_TEST_FLAGS -target="$TARGET" $clean_flags \
                -S "$src" -o "$asm" >"$err" 2>&1 || ok=0
            [ "$ok" -eq 1 ] && {
                if [ -n "$clang_target" ]; then
                    verbose_cmd "$ASSEMBLER -target $clang_target -c $asm -o $obj"
                    "$ASSEMBLER" -target "$clang_target" -c "$asm" -o "$obj" >>"$err" 2>&1 || ok=0
                else
                    verbose_cmd "$ASSEMBLER -c $asm -o $obj"
                    "$ASSEMBLER" -c "$asm" -o "$obj" >>"$err" 2>&1 || ok=0
                fi
            }

            if [ "$ok" -eq 1 ]; then
                for ef in $(printf '%s' "$clang_extra_src" | tr ',' ' '); do
                    extra_src_path="tests/$ef"
                    extra_obj="$TMPDIR/${safename}_$(printf '%s' "$ef" | tr '/. ' '___').o"
                    if [ -n "$clang_target" ]; then
                        verbose_cmd "$ASSEMBLER -target $clang_target -c $extra_src_path -o $extra_obj"
                        "$ASSEMBLER" -target "$clang_target" -c "$extra_src_path" -o "$extra_obj" >>"$err" 2>&1 || ok=0
                    else
                        verbose_cmd "$ASSEMBLER -c $extra_src_path -o $extra_obj"
                        "$ASSEMBLER" -c "$extra_src_path" -o "$extra_obj" >>"$err" 2>&1 || ok=0
                    fi
                    link_inputs="$link_inputs $extra_obj"
                done
            fi

            [ "$ok" -eq 1 ] && {
                if [ -n "$clang_target" ]; then
                    verbose_cmd "$ASSEMBLER -target $clang_target $link_inputs -o $exe"
                    # shellcheck disable=SC2086
                    "$ASSEMBLER" -target "$clang_target" $link_inputs -o "$exe" >>"$err" 2>&1 || ok=0
                else
                    verbose_cmd "$ASSEMBLER $link_inputs -o $exe"
                    # shellcheck disable=SC2086
                    "$ASSEMBLER" $link_inputs -o "$exe" >>"$err" 2>&1 || ok=0
                fi
            }
        elif [ -n "$extra_src" ]; then
            extra_srcs=""
            for ef in $(printf '%s' "$extra_src" | tr ',' ' '); do
                extra_srcs="$extra_srcs tests/$ef"
            done
            verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$TARGET $clean_flags $src $extra_srcs -o $exe"
            # shellcheck disable=SC2086
            "$COMPILER" $TCC_TEST_FLAGS -target="$TARGET" $clean_flags \
                "$src" $extra_srcs -o "$exe" >"$err" 2>&1 || ok=0
        else
            verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$TARGET $clean_flags -S $src -o $asm"
            # shellcheck disable=SC2086
            "$COMPILER" $TCC_TEST_FLAGS -target="$TARGET" $clean_flags \
                -S "$src" -o "$asm" >"$err" 2>&1 || ok=0
            [ "$ok" -eq 1 ] && {
                if [ -n "$clang_target" ]; then
                    verbose_cmd "$ASSEMBLER -target $clang_target $asm -o $exe"
                    "$ASSEMBLER" -target "$clang_target" "$asm" -o "$exe" >>"$err" 2>&1 || ok=0
                else
                    verbose_cmd "$ASSEMBLER $asm -o $exe"
                    "$ASSEMBLER" "$asm" -o "$exe" >>"$err" 2>&1 || ok=0
                fi
            }
        fi

        if [ "$type" = "linkerror" ]; then
            if [ "$ok" -eq 0 ]; then
                if [ -n "$expected" ] && ! grep -qF "$expected" "$err"; then
                    [ "$last_newline" -eq 0 ] && echo
                    printf "FAIL %s (missing: %s)\n" "$base" "$expected"
                    [ "$VERBOSE" -eq 1 ] && cat "$err" >&2
                    fail=$((fail + 1))
                    last_newline=1
                else
                    pass=$((pass + 1))
                    if ! verbose_status "PASS"; then
                        printf "."; last_newline=0
                    fi
                fi
            else
                [ "$last_newline" -eq 0 ] && echo
                printf "FAIL %s (link succeeded; expected failure)\n" "$base"
                fail=$((fail + 1))
                last_newline=1
            fi
            continue
        fi

        if [ "$ok" -eq 0 ]; then
            [ "$last_newline" -eq 0 ] && echo
            if [ "$type" = "todo" ]; then
                printf "SKIP %s (compile/link failed)\n" "$base"
                [ -n "$desc" ] && printf "     note: %s\n" "$desc"
                skip=$((skip + 1))
            else
                printf "FAIL %s (compile/link error)\n" "$base"
                [ "$VERBOSE" -eq 1 ] && cat "$err" >&2
                fail=$((fail + 1))
            fi
            last_newline=1
            continue
        fi

        out="$TMPDIR/${safename}.out"
        verbose_cmd "$exe"
        "$exe" >"$out" 2>>"$err"; actual=$?
        stdout_ok=1
        stdout_error=""
        if [ "$actual" = "$expected" ] && [ -n "$expected_stdout" ]; then
            expected_stdout_path="tests/$expected_stdout"
            if [ ! -f "$expected_stdout_path" ]; then
                stdout_ok=0
                stdout_error="expected stdout file not found: $expected_stdout"
            elif ! cmp -s "$out" "$expected_stdout_path"; then
                stdout_ok=0
                stdout_error="stdout differs from $expected_stdout"
            fi
        fi
        if [ "$actual" = "$expected" ] && [ "$stdout_ok" -eq 1 ]; then
            pass=$((pass + 1))
            if [ "$type" = "todo" ]; then
                [ "$last_newline" -eq 0 ] && echo
                printf "PASS %s (todo now passing — promote to run)\n" "$base"
                last_newline=1
            elif ! verbose_status "PASS"; then
                printf "."; last_newline=0
            else
                :
            fi
        else
            [ "$last_newline" -eq 0 ] && echo
            if [ "$type" = "todo" ]; then
                if [ "$actual" != "$expected" ]; then
                    printf "SKIP %s (expected=%s actual=%s)\n" "$base" "$expected" "$actual"
                else
                    printf "SKIP %s (%s)\n" "$base" "$stdout_error"
                fi
                [ -n "$desc" ] && printf "     note: %s\n" "$desc"
                skip=$((skip + 1))
            else
                if [ "$actual" != "$expected" ]; then
                    printf "FAIL %s (expected=%s actual=%s)\n" "$base" "$expected" "$actual"
                else
                    printf "FAIL %s (%s)\n" "$base" "$stdout_error"
                    [ "$VERBOSE" -eq 1 ] && diff -u "$expected_stdout_path" "$out" >&2 || :
                fi
                fail=$((fail + 1))
            fi
            last_newline=1
        fi
        ;;

    error)
        diag="$TMPDIR/${safename}.diag"
        error_phase_flags="$flags"
        case "$src" in
            *.c)
                case " $flags " in
                    *" -S "*|*" -E "*|*" -c "*) ;;
                    *) error_phase_flags="-S $flags" ;;
                esac
                ;;
        esac
        verbose_cmd "$COMPILER $TCC_TEST_FLAGS $error_phase_flags $src -o /dev/null"
        # shellcheck disable=SC2086
        "$COMPILER" $TCC_TEST_FLAGS $error_phase_flags "$src" -o /dev/null >"$diag" 2>&1
        status=$?

        if [ "$status" -eq 0 ]; then
            [ "$last_newline" -eq 0 ] && echo
            printf "FAIL %s (compiler succeeded; expected failure)\n" "$base"
            last_newline=1; fail=$((fail + 1))
            continue
        fi

        if [ -n "$expected" ] && ! grep -qF "$expected" "$diag"; then
            [ "$last_newline" -eq 0 ] && echo
            printf "FAIL %s (missing: %s)\n" "$base" "$expected"
            [ "$VERBOSE" -eq 1 ] && cat "$diag" >&2
            last_newline=1; fail=$((fail + 1))
        else
            pass=$((pass + 1))
            if ! verbose_status "PASS"; then
                printf "."; last_newline=0
            fi
        fi
        ;;

    warn)
        warn_err="$TMPDIR/${safename}.warn"
        asm="$TMPDIR/${safename}.s"
        verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$TARGET $flags -S $src -o $asm"
        # shellcheck disable=SC2086
        "$COMPILER" $TCC_TEST_FLAGS -target="$TARGET" $flags \
            -S "$src" -o "$asm" >"$warn_err" 2>&1
        stderr_out=$(cat "$warn_err")
        if [ -n "$expected" ]; then
            if printf '%s' "$stderr_out" | grep -qF "$expected"; then
                pass=$((pass + 1))
                if ! verbose_status "PASS"; then
                    printf "."; last_newline=0
                fi
            else
                [ "$last_newline" -eq 0 ] && echo
                printf "FAIL %s (expected warning '%s', got none)\n" "$base" "$expected"
                last_newline=1; fail=$((fail + 1))
            fi
        else
            if [ -n "$stderr_out" ]; then
                printf "."; last_newline=0; pass=$((pass + 1))
            else
                [ "$last_newline" -eq 0 ] && echo
                printf "FAIL %s (expected any warning, got none)\n" "$base"
                last_newline=1; fail=$((fail + 1))
            fi
        fi
        ;;

    nowarn)
        nowarn_err="$TMPDIR/${safename}.nowarn"
        asm="$TMPDIR/${safename}.s"
        verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$TARGET $flags -S $src -o $asm"
        # shellcheck disable=SC2086
        "$COMPILER" $TCC_TEST_FLAGS -target="$TARGET" $flags \
            -S "$src" -o "$asm" >"$nowarn_err" 2>&1
        stderr_out=$(cat "$nowarn_err")
        if [ -z "$stderr_out" ]; then
            pass=$((pass + 1))
            if ! verbose_status "PASS"; then
                printf "."; last_newline=0
            fi
        else
            [ "$last_newline" -eq 0 ] && echo
            printf "FAIL %s (unexpected stderr: %s)\n" "$base" "$stderr_out"
            last_newline=1; fail=$((fail + 1))
        fi
        ;;

    dwarf)
        # Per-line flags may contain -target=X to override global target
        dwarf_target="$TARGET"
        dwarf_flags=""
        for f in $flags; do
            case "$f" in
                -target=*) dwarf_target="${f#-target=}" ;;
                *)         dwarf_flags="$dwarf_flags $f" ;;
            esac
        done

        cached=$(asm_cache_get "$path" "$flags")
        if [ -z "$cached" ]; then
            cached="$TMPDIR/dwarf_${safename}_$(asm_key '' "$flags").s"
            derr="$TMPDIR/dwarf_${safename}.err"
            verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$dwarf_target $dwarf_flags -S $src -o $cached"
            # shellcheck disable=SC2086
            if ! "$COMPILER" $TCC_TEST_FLAGS -target="$dwarf_target" $dwarf_flags \
                    -S "$src" -o "$cached" >"$derr" 2>&1; then
                [ "$last_newline" -eq 0 ] && echo
                printf "FAIL %s (compiler error)\n" "$base"
                [ "$VERBOSE" -eq 1 ] && cat "$derr" >&2
                last_newline=1; fail=$((fail + 1))
                continue
            fi
            asm_cache_set "$path" "$flags" "$cached"
        fi

        failed_pat=""
        for pat in $expected; do
            if ! grep -q "$pat" "$cached"; then
                failed_pat="$pat"; break
            fi
        done

        if [ -z "$failed_pat" ]; then
            pass=$((pass + 1))
            if ! verbose_status "PASS"; then
                printf "."; last_newline=0
            fi
        else
            [ "$last_newline" -eq 0 ] && echo
            printf "FAIL %s [%s] (missing: %s)\n" "$base" "$dwarf_target" "$failed_pat"
            last_newline=1; fail=$((fail + 1))
        fi
        ;;


    dwarfverify)
        # Compile an object with DWARF and run dwarfdump/llvm-dwarfdump --verify.
        # This catches malformed DIE references that assembly-pattern tests cannot see.
        dwarf_target="$TARGET"
        dwarf_flags=""
        for f in $flags; do
            case "$f" in
                -target=*) dwarf_target="${f#-target=}" ;;
                *)         dwarf_flags="$dwarf_flags $f" ;;
            esac
        done

        obj="$TMPDIR/dwarfverify_${safename}.o"
        derr="$TMPDIR/dwarfverify_${safename}.err"
        vout="$TMPDIR/dwarfverify_${safename}.verify"

        verbose_cmd "$COMPILER $TCC_TEST_FLAGS -target=$dwarf_target $dwarf_flags -c $src -o $obj"
        # shellcheck disable=SC2086
        if ! "$COMPILER" $TCC_TEST_FLAGS -target="$dwarf_target" $dwarf_flags \
                -c "$src" -o "$obj" >"$derr" 2>&1; then
            [ "$last_newline" -eq 0 ] && echo
            printf "FAIL %s (compiler error)\n" "$base"
            [ "$VERBOSE" -eq 1 ] && cat "$derr" >&2
            last_newline=1; fail=$((fail + 1))
            continue
        fi

        verifier=""
        if command -v dwarfdump >/dev/null 2>&1; then
            verifier="dwarfdump"
        elif command -v llvm-dwarfdump >/dev/null 2>&1; then
            verifier="llvm-dwarfdump"
        fi

        if [ -z "$verifier" ]; then
            [ "$last_newline" -eq 0 ] && echo
            printf "SKIP %s (no dwarfdump verifier found)\n" "$base"
            skip=$((skip + 1))
            last_newline=1
            continue
        fi

        verbose_cmd "$verifier --verify $obj"
        if "$verifier" --verify "$obj" >"$vout" 2>&1; then
            pass=$((pass + 1))
            if ! verbose_status "PASS"; then
                printf "."; last_newline=0
            fi
        else
            [ "$last_newline" -eq 0 ] && echo
            printf "FAIL %s (DWARF verification failed)\n" "$base"
            cat "$vout" >&2
            last_newline=1; fail=$((fail + 1))
        fi
        ;;

    *)
        [ "$last_newline" -eq 0 ] && echo
        printf "FAIL %s (unknown type: %s)\n" "$base" "$type"
        last_newline=1; fail=$((fail + 1))
        ;;

    esac

done < "$SORTED_MANIFEST"

[ "$last_newline" -eq 0 ] && echo
echo "------------------------------"
echo "$tests/$tests Completed"
echo "$fail Failed"
[ "$skip" -gt 0 ] && echo "$skip Skipped"
echo "------------------------------"

exit $fail

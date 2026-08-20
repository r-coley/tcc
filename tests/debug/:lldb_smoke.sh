#!/bin/sh
# :lldb_smoke.sh — LLDB smoke validation for tcc-generated debug info.
#
# This is intentionally separate from the normal test suite because it depends
# on an installed LLDB and a host/target combination that can execute the
# produced binary.
#
# This gated smoke check validates both debug workflows:
#   - persistent object flow: tcc -g -c file.c; clang -g file.o -o exe
#   - one-shot flow:         tcc -g file.c -o exe
#
# Both flows must let LLDB resolve a breakpoint, stop in debug_target, show
# source/line information, inspect simple parameters/locals, and exit cleanly.

COMPILER="build/tcc_stage0"
TARGET=""
ASSEMBLER="clang"
TMPDIR="build/tmp/lldb-smoke"
FLAGS=""

while [ $# -gt 0 ]; do
    case "$1" in
        -c) COMPILER="$2"; shift 2 ;;
        -t) TARGET="$2"; shift 2 ;;
        -a) ASSEMBLER="$2"; shift 2 ;;
        -T) TMPDIR="$2"; shift 2 ;;
        --flags) FLAGS="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: tests/debug/:lldb_smoke.sh -c COMPILER -t TARGET -a ASSEMBLER -T TMPDIR [--flags FLAGS]"
            exit 0
            ;;
        *)
            echo ":lldb_smoke.sh: unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

if [ -z "$TARGET" ]; then
    case "$(uname -m 2>/dev/null)" in
        arm64|aarch64) TARGET="arm64" ;;
        x86_64|amd64)  TARGET="x64" ;;
        *)
            echo ":lldb_smoke.sh: cannot auto-detect target; pass -t arm64|x64" >&2
            exit 1
            ;;
    esac
fi

if [ ! -x "$COMPILER" ]; then
    echo ":lldb_smoke.sh: compiler not found or not executable: $COMPILER" >&2
    exit 1
fi

if ! command -v lldb >/dev/null 2>&1; then
    echo "LLDB not found; skipping source-level debug smoke validation."
    exit 0
fi

mkdir -p "$TMPDIR"

SRC="tests/debug/lldb_smoke.c"
OBJ="$TMPDIR/lldb_smoke.o"
EXE="$TMPDIR/lldb_smoke"
EXE_ONESHOT="$TMPDIR/lldb_smoke_oneshot"
STRUCT_SRC="tests/debug/debug_struct_local.c"
STRUCT_EXE="$TMPDIR/lldb_struct"
ARRAY_SRC="tests/debug/debug_array_local.c"
ARRAY_EXE="$TMPDIR/lldb_array"
STRUCT_PTR_SRC="tests/debug/debug_struct_ptr_local.c"
STRUCT_PTR_EXE="$TMPDIR/lldb_struct_ptr"
STRUCT_PTR_PTR_SRC="tests/debug/debug_struct_ptr_ptr_local.c"
STRUCT_PTR_PTR_EXE="$TMPDIR/lldb_struct_ptr_ptr"
PARAM_STRUCT_PTR_SRC="tests/debug/debug_param_struct_ptr.c"
PARAM_STRUCT_PTR_EXE="$TMPDIR/lldb_param_struct_ptr"
PARAM_STRUCT_VALUE_SRC="tests/debug/debug_param_struct_value.c"
PARAM_STRUCT_VALUE_EXE="$TMPDIR/lldb_param_struct_value"
STRUCT_ARRAY_SRC="tests/debug/debug_struct_array_local.c"
STRUCT_ARRAY_EXE="$TMPDIR/lldb_struct_array"
STRUCT_MEMBER_ARRAY_SRC="tests/debug/debug_struct_member_array.c"
STRUCT_MEMBER_ARRAY_EXE="$TMPDIR/lldb_struct_member_array"
NESTED_STRUCT_SRC="tests/debug/debug_nested_struct_member.c"
NESTED_STRUCT_EXE="$TMPDIR/lldb_nested_struct_member"
CMDS="$TMPDIR/lldb.commands"
OUT="$TMPDIR/lldb.out"
ERR="$TMPDIR/lldb.err"

printf "Running LLDB source-level debug smoke test:\n"
printf "  compiler:  %s\n" "$COMPILER"
printf "  target:    %s\n" "$TARGET"
printf "  assembler: %s\n" "$ASSEMBLER"
[ -n "$FLAGS" ] && printf "  flags:     %s\n" "$FLAGS"
printf "\n"

run_lldb_param_struct_ptr_case()
{
    exe="$1"
    label="$2"
    out="$TMPDIR/lldb.out"
    cat > "$CMDS" <<EOF
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_param_struct_ptr_probe
run
frame variable p
frame variable scale
p p->x
p p->y
p scale
continue
quit
EOF
    if ! lldb --batch -s "$CMDS" >"$out" 2>&1; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$out" >&2
        exit 1
    fi
    check_output "$label breakpoint creation" 'Breakpoint [0-9]+:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint'
    check_output "$label source line"         'debug_param_struct_ptr\.c:[0-9]'
    check_output "$label frame"               'debug_param_struct_ptr_probe'
    check_output "$label typed pointer param" 'TccDbgPoint \*\) p = '
    check_output "$label int param"           '\(int\) scale = 12'
    check_output "$label member x"            '\(int\) 10'
    check_output "$label member y"            '\(int\) 20'
    check_output "$label clean process exit"  'exited with status = 0'
}

run_lldb_param_struct_value_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --file debug_param_struct_value.c --line 8
run
frame variable p
frame variable scale
p p.x
p p.y
p scale
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,260p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_param_struct_value\.c:[0-9]'
    check_output "$label frame"               'debug_param_struct_value_probe'
    check_output "$label struct parameter"    'TccDbgPoint|struct TccDbgPoint|x = 10'
    check_output "$label int parameter"       '\(int\) scale = 12|scale = 12'
    check_output "$label member x"            '\(int\) 10|x = 10'
    check_output "$label member y"            '\(int\) 20|y = 20'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}


# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g -c "$SRC" -o "$OBJ" 2>"$ERR"; then
    echo "FAIL lldb-smoke (compiler error)"
    cat "$ERR" >&2
    exit 1
fi

if ! "$ASSEMBLER" -g "$OBJ" -o "$EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (link error)"
    cat "$ERR" >&2
    exit 1
fi

# Run dsymutil to extract DWARF from .o into .dSYM bundle
if command -v dsymutil >/dev/null 2>&1; then
    if ! dsymutil "$EXE" 2>"$ERR"; then
        echo "FAIL lldb-smoke (dsymutil error)"
        cat "$ERR" >&2
        exit 1
    fi
fi

check_output()
{
    label="$1"
    pattern="$2"
    if ! grep -Eq "$pattern" "$OUT"; then
        echo "FAIL lldb-smoke (missing $label)"
        sed -n '1,220p' "$OUT" >&2
        exit 1
    fi
}


build_debug_exe()
{
    src="$1"
    exe="$2"
    stem="$3"

    case "$(uname -s 2>/dev/null)" in
        Darwin)
            case "$TARGET" in
                arm64) clang_target="arm64-apple-macos" ;;
                x64|x86_64) clang_target="x86_64-apple-macos" ;;
                *) clang_target="" ;;
            esac

            if [ -n "$clang_target" ] && command -v dsymutil >/dev/null 2>&1; then
                asm="$TMPDIR/$stem.s"
                obj="$TMPDIR/$stem.o"

                # Keep the object file alive until after dsymutil has consumed
                # the executable's debug map.  The direct one-shot compiler path
                # may delete temporary objects too early for dsymutil on Darwin.
                # shellcheck disable=SC2086 # FLAGS is intentionally word-split.
                if ! "$COMPILER" $FLAGS -target="$TARGET" -g -S "$src" -o "$asm" 2>"$ERR"; then
                    echo "FAIL lldb-smoke ($stem compiler assembly error)"
                    cat "$ERR" >&2
                    exit 1
                fi

                if ! "$ASSEMBLER" -target "$clang_target" -c "$asm" -o "$obj" 2>"$ERR"; then
                    echo "FAIL lldb-smoke ($stem assemble error)"
                    cat "$ERR" >&2
                    exit 1
                fi

                if ! "$ASSEMBLER" "$obj" -o "$exe" 2>"$ERR"; then
                    echo "FAIL lldb-smoke ($stem link error)"
                    cat "$ERR" >&2
                    exit 1
                fi

                if ! dsymutil "$exe" 2>"$ERR"; then
                    echo "FAIL lldb-smoke ($stem dsymutil error)"
                    cat "$ERR" >&2
                    exit 1
                fi
                return 0
            fi
            ;;
    esac

    # Fallback path for non-Darwin hosts, or unsupported target names.
    # shellcheck disable=SC2086 # FLAGS is intentionally word-split.
    if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$src" -o "$exe" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($stem compiler/link error)"
        cat "$ERR" >&2
        exit 1
    fi
}




run_lldb_nested_struct_member_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_nested_struct_member_probe
run
next
next
next
frame variable box
p box.origin
p box.origin.x
p box.origin.y
p box.scale
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,260p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_nested_struct_member\.c:[0-9]'
    check_output "$label frame"               'debug_nested_struct_member_probe'
    check_output "$label outer struct"        'TccDbgBox|struct TccDbgBox'
    check_output "$label nested struct"       'TccDbgPoint|struct TccDbgPoint|origin = \(x = 10, y = 20\)'
    check_output "$label origin x"            '\(int\) 10|x = 10'
    check_output "$label origin y"            '\(int\) 20|y = 20'
    check_output "$label scale"               '\(int\) 12|scale = 12'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_struct_member_array_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_struct_member_array_probe
run
next
next
next
frame variable line
p line.values
p line.values[0]
p line.values[1]
p line.values[2]
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,240p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_struct_member_array\.c:[0-9]'
    check_output "$label frame"               'debug_struct_member_array_probe'
    check_output "$label struct variable"     'TccDbgLine|struct TccDbgLine'
    check_output "$label member array"        'int \[3\]|int\[3\]|\[0\].*10'
    check_output "$label element 0"           '\(int\) 10|\[0\] = 10'
    check_output "$label element 1"           '\(int\) 20|\[1\] = 20'
    check_output "$label element 2"           '\(int\) 12|\[2\] = 12'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_struct_array_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_struct_array_probe
run
next
next
next
next
frame variable pts
p pts
p pts[0].x
p pts[0].y
p pts[1].x
p pts[1].y
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,240p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_struct_array_local\.c:[0-9]'
    check_output "$label struct-array frame"  'debug_struct_array_probe'
    check_output "$label array variable"      'TccDbgPoint \[2\]|TccDbgPoint\[2\]|struct TccDbgPoint \[2\]|\[0\].*x = 10'
    check_output "$label element 0 x"         'x = 10|\(int\) 10'
    check_output "$label element 0 y"         'y = 11|\(int\) 11'
    check_output "$label element 1 x"         'x = 20|\(int\) 20'
    check_output "$label element 1 y"         'y = 1|\(int\) 1'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_struct_ptr_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_struct_ptr_probe
run
next
next
next
frame variable p
frame variable q
p p.x
p p.y
p q
p q->x
p q->y
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,220p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_struct_ptr_local\.c:[0-9]'
    check_output "$label struct-ptr frame"    'debug_struct_ptr_probe'
    check_output "$label struct variable"     'TccDbgPoint|struct TccDbgPoint'
    check_output "$label pointer variable"    'TccDbgPoint \*|struct TccDbgPoint \*'
    check_output "$label member x"            'x = 19|\(int\) 19'
    check_output "$label member y"            'y = 23|\(int\) 23'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_array_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_array_probe
run
next
next
next
frame variable a
p a
p a[0]
p a[1]
p a[2]
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,220p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_array_local\.c:[0-9]'
    check_output "$label array frame"         'debug_array_probe'
    check_output "$label array variable"      'int \[3\]|int\[3\]|\[0\].*10'
    check_output "$label element 0"           '\(int\) 10|\[0\] = 10'
    check_output "$label element 1"           '\(int\) 20|\[1\] = 20'
    check_output "$label element 2"           '\(int\) 12|\[2\] = 12'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_struct_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_struct_probe
run
next
next
frame variable p
p p.x
p p.y
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,220p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_struct_local\.c:[0-9]'
    check_output "$label struct frame"        'debug_struct_probe'
    check_output "$label struct variable"     'TccDbgPoint|struct TccDbgPoint'
    check_output "$label member x"            'x = 19|\(int\) 19'
    check_output "$label member y"            'y = 23|\(int\) 23'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_struct_ptr_ptr_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_struct_ptr_ptr_probe
run
next
next
next
next
frame variable p
frame variable q
frame variable qq
p q->x
p q->y
p (*qq)->x
p (*qq)->y
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,260p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation" 'Breakpoint 1:'
    check_output "$label breakpoint stop"     'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"         'debug_struct_ptr_ptr_local\.c:[0-9]'
    check_output "$label frame"               'debug_struct_ptr_ptr_probe'
    check_output "$label local struct"        'TccDbgPoint'
    check_output "$label pointer local"       'TccDbgPoint \*\) q|TccDbgPoint \* q'
    check_output "$label pointer-pointer"     'TccDbgPoint \*\*\) qq|TccDbgPoint \*\* qq'
    check_output "$label q member x"          '\(int\) 19|x = 19'
    check_output "$label q member y"          '\(int\) 23|y = 23'
    check_output "$label clean process exit"  'exited with status = 0|status = 0'
}

run_lldb_case()
{
    exe="$1"
    label="$2"

    cat > "$CMDS" <<EOF_CMDS
settings set target.disable-aslr false
target create "$exe"
breakpoint set --name debug_target
run
frame variable
thread backtrace
continue
quit
EOF_CMDS

    if ! lldb --batch -s "$CMDS" >"$OUT" 2>"$ERR"; then
        echo "FAIL lldb-smoke ($label lldb command failure)"
        cat "$ERR" >&2
        sed -n '1,180p' "$OUT" >&2
        exit 1
    fi

    check_output "$label breakpoint creation"    'Breakpoint 1:'
    check_output "$label breakpoint stop"        'stop reason = breakpoint|stop reason = Breakpoint'
    check_output "$label source line"            'lldb_smoke\.c:[0-9]'
    check_output "$label debug_target frame"     'debug_target'
    check_output "$label frame variable seed"    '\(int\) seed'
    check_output "$label frame variable doubled" '\(int\) doubled'
    check_output "$label frame variable total"   '\(int\) total'
    check_output "$label backtrace main"         'main.*lldb_smoke\.c'
    check_output "$label clean process exit"     'exited with status = 0|status = 0'
}

run_lldb_case "$EXE" "object-link"
printf "."

# Verify one-shot compile+link retains usable debug info after temporary
# objects have been cleaned up.  On macOS this requires the compiler driver to
# run dsymutil before removing its temporary .o files, or to preserve them as a
# fallback.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$SRC" -o "$EXE_ONESHOT" 2>"$ERR"; then
    echo "FAIL lldb-smoke (one-shot compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_case "$EXE_ONESHOT" "one-shot"
printf "."

# Validate that a simple local struct variable is described with a struct DIE
# and member DIEs, not as a plain int pointing at the first field.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$STRUCT_SRC" -o "$STRUCT_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (struct debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_struct_case "$STRUCT_EXE" "struct-local"
printf "."

# Validate that a simple local array variable is described with an array DIE
# and subrange DIE, not as a plain int pointing at the first element.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$ARRAY_SRC" -o "$ARRAY_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (array debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_array_case "$ARRAY_EXE" "array-local"
printf "."

# Validate that a pointer-to-struct local is described as a pointer DIE whose
# target type is the struct DIE.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$STRUCT_PTR_SRC" -o "$STRUCT_PTR_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (struct-pointer debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_struct_ptr_case "$STRUCT_PTR_EXE" "struct-pointer-local"
printf "."

# Validate that an array of structs uses an array DIE whose element type
# references the struct DIE, not a builtin int fallback.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$STRUCT_ARRAY_SRC" -o "$STRUCT_ARRAY_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (struct-array debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_struct_array_case "$STRUCT_ARRAY_EXE" "struct-array-local"
printf "."


# Validate that a struct member array is described as an array DIE referenced
# by the member DIE, not as a scalar int fallback.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$STRUCT_MEMBER_ARRAY_SRC" -o "$STRUCT_MEMBER_ARRAY_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (struct-member-array debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_struct_member_array_case "$STRUCT_MEMBER_ARRAY_EXE" "struct-member-array"
printf "."


# Validate that a struct member whose type is another struct references the
# nested struct DIE, not a scalar int fallback.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$NESTED_STRUCT_SRC" -o "$NESTED_STRUCT_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (nested-struct-member debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_nested_struct_member_case "$NESTED_STRUCT_EXE" "nested-struct-member"
printf "."


# Validate that pointer-to-struct pointer locals are described as pointer DIE
# chains, not collapsed to a single pointer-to-struct DIE.
# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$STRUCT_PTR_PTR_SRC" -o "$STRUCT_PTR_PTR_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (struct-pointer-pointer debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_struct_ptr_ptr_case "$STRUCT_PTR_PTR_EXE" "struct-pointer-pointer-local"
printf "."


# shellcheck disable=SC2086 # FLAGS is intentionally word-split.
if ! "$COMPILER" $FLAGS -target="$TARGET" -g "$PARAM_STRUCT_PTR_SRC" -o "$PARAM_STRUCT_PTR_EXE" 2>"$ERR"; then
    echo "FAIL lldb-smoke (param struct pointer debug compiler/link error)"
    cat "$ERR" >&2
    exit 1
fi

run_lldb_param_struct_ptr_case "$PARAM_STRUCT_PTR_EXE" "param-struct-pointer"
printf "."

# Validate that by-value struct parameters resolve through the source-level
# local copy, not through the hidden ABI pointer slot.  On Darwin, preserve the
# object file through dsymutil so LLDB can see the relocated debug info.
build_debug_exe "$PARAM_STRUCT_VALUE_SRC" "$PARAM_STRUCT_VALUE_EXE" "lldb_param_struct_value"

run_lldb_param_struct_value_case "$PARAM_STRUCT_VALUE_EXE" "param-struct-value"
printf "."

printf "\n"
echo "------------------------------"
echo "11/11 Checked"
echo "0 Failed"
echo "------------------------------"

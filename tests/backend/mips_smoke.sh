#!/bin/sh
set -u

CC=
TMP=
TEST_DIR=tests

while [ $# -gt 0 ]; do
    case "$1" in
        -c) CC=$2; shift 2 ;;
        -T) TMP=$2; shift 2 ;;
        -d) TEST_DIR=$2; shift 2 ;;
        --flags) shift 2 ;; # accepted for consistency with other smoke scripts
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

compile_one() {
    src=$1
    base=$(basename "$src")
    if "$CC" -S -target=mips "$src" -o "$TMP/$base.mips.s" >/dev/null 2>"$TMP/$base.mips.err" && [ -s "$TMP/$base.mips.s" ]; then
        echo "  PASS $base"
    else
        echo "  FAIL $base"
        cat "$TMP/$base.mips.err"
        fail=$((fail + 1))
    fi
}

check() {
    msg=$1
    shift
    if "$@"; then
        echo "  CHECK $msg"
    else
        echo "  FAIL $msg"
        fail=$((fail + 1))
    fi
}

fail() {
    echo "  FAIL $1"
    fail=$((fail + 1))
    return 1
}

echo "MIPS assembly-generation smoke test:"

for src in \
    "$TEST_DIR/core/test001.c" \
    "$TEST_DIR/core/test002.c" \
    "$TEST_DIR/backend/mips_call_args.c" \
    "$TEST_DIR/backend/mips_global_initializers.c" \
    "$TEST_DIR/backend/mips_local_stack.c" \
    "$TEST_DIR/backend/mips_branch_loop.c" \
    "$TEST_DIR/backend/mips_pointer_load_store.c" \
    "$TEST_DIR/backend/mips_static_locals.c" \
    "$TEST_DIR/backend/mips_global_arrays.c" \
    "$TEST_DIR/backend/mips_pointer_arith.c" \
    "$TEST_DIR/backend/mips_signed_unsigned_cmp.c" \
    "$TEST_DIR/backend/mips_short_circuit.c" \
    "$TEST_DIR/backend/mips_nested_break_continue.c" \
    "$TEST_DIR/backend/mips_funcptr_basic.c" \
    "$TEST_DIR/backend/mips_div_mod_signed.c" \
    "$TEST_DIR/backend/mips_div_mod_unsigned.c" \
    "$TEST_DIR/backend/mips_conditional_expr.c" \
    "$TEST_DIR/backend/mips_switch_basic.c" \
    "$TEST_DIR/backend/mips_addr_of_local.c" \
    "$TEST_DIR/backend/mips_narrow_load_store.c" \
    "$TEST_DIR/backend/mips_recursive_calls.c" \
    "$TEST_DIR/backend/mips_nested_call_args.c" \
    "$TEST_DIR/backend/mips_mutual_calls.c" \
    "$TEST_DIR/backend/mips_local_array_index.c" \
    "$TEST_DIR/backend/mips_pre_post_inc.c" \
    "$TEST_DIR/backend/mips_pointer_inc_store.c" \
    "$TEST_DIR/backend/mips_local_struct_fields.c" \
    "$TEST_DIR/backend/mips_struct_ptr_fields.c" \
    "$TEST_DIR/backend/mips_global_struct_fields.c" \
    "$TEST_DIR/backend/mips_array_of_structs.c" \
    "$TEST_DIR/backend/mips_nested_struct_fields.c" \
    "$TEST_DIR/backend/mips_string_global_ptr.c" \
    "$TEST_DIR/backend/mips_mixed_width_locals.c" \
    "$TEST_DIR/backend/mips_funcptr_reassign.c" \
    "$TEST_DIR/backend/mips_funcptr_global.c" \
    "$TEST_DIR/backend/mips_indirect_call_args.c" \
    "$TEST_DIR/backend/mips_funcptr_struct.c" \
    "$TEST_DIR/backend/mips_struct_arg_by_value.c" \
    "$TEST_DIR/backend/mips_struct_arg_mixed_scalars.c" \
    "$TEST_DIR/backend/mips_struct_return_small.c" \
    "$TEST_DIR/backend/mips_struct_return_medium.c" \
    "$TEST_DIR/backend/mips_varargs_sum.c" \
    "$TEST_DIR/backend/mips_varargs_mixed.c" \
    "$TEST_DIR/backend/mips_i64_add_sub.c" \
    "$TEST_DIR/backend/mips_i64_compare.c" \
    "$TEST_DIR/backend/mips_i64_shifts.c" \
    "$TEST_DIR/backend/mips_i64_bitwise.c" \
    "$TEST_DIR/backend/mips_i64_mul_div_mod.c"

# NOTE: MIPS i64 probes are compile-only for now.
# Current MIPS lowering treats long long mostly as one 32-bit word.
# Do not add CHECK rules for these until real 64-bit split-word lowering exists.
do
    compile_one "$src"
done

check "mips_call_args.c call lowering" sh -c '
    grep -q "mix" "$1" &&
    grep -q "jal mix" "$1" &&
    grep -q "prepare 5 call args" "$1" &&
    grep -q "stack parameter 4" "$1"
' sh "$TMP/mips_call_args.c.mips.s"

check "mips_global_initializers.c scalar data directives" sh -c \
    "grep -q 'gi' '$TMP/mips_global_initializers.c.mips.s' &&
     grep -q 'gc' '$TMP/mips_global_initializers.c.mips.s' &&
     grep -q 'gs' '$TMP/mips_global_initializers.c.mips.s' &&
     grep -Eq '\\.word|\\.long|\\.byte|\\.half|\\.short' '$TMP/mips_global_initializers.c.mips.s'"

check "mips_local_stack.c stack locals" sh -c '
    grep -q "li" "$1" &&
    grep -q "17" "$1" &&
    grep -q "25" "$1" &&
    grep -q "sw" "$1" &&
    grep -q "lw" "$1" &&
    grep -q "addu" "$1"
' sh "$TMP/mips_local_stack.c.mips.s"

check "mips_branch_loop.c branch/loop lowering" sh -c \
    "grep -Eq 'beq|bne|bgt|bge|blt|ble|j[[:space:]]' '$TMP/mips_branch_loop.c.mips.s' &&
     grep -q '5' '$TMP/mips_branch_loop.c.mips.s' &&
     grep -q '3' '$TMP/mips_branch_loop.c.mips.s'"

check "mips_pointer_load_store.c pointer load/store" sh -c \
    "grep -Eq 'lw|sw' '$TMP/mips_pointer_load_store.c.mips.s' &&
     grep -Eq 'addiu|addu|la|move' '$TMP/mips_pointer_load_store.c.mips.s' &&
     grep -q '42' '$TMP/mips_pointer_load_store.c.mips.s'"


check "mips_static_locals.c static storage load/store" sh -c \
    "grep -q '_counter' '$TMP/mips_static_locals.c.mips.s' &&
     grep -q '\.long 40' '$TMP/mips_static_locals.c.mips.s' &&
     grep -q 'la \$t0, counter' '$TMP/mips_static_locals.c.mips.s' &&
     grep -q 'sw \$v0, 0(\$t0)' '$TMP/mips_static_locals.c.mips.s' &&
     grep -q 'jal bump' '$TMP/mips_static_locals.c.mips.s'"

check "mips_global_arrays.c global array data/indexing" sh -c \
    "grep -q '_gi' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q '_gb' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q '_gw' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q '\.long 20' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q '\.byte 1, 2, 3, 4' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q '\.short 6' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q 'sll \$v0, \$v0, 2' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q 'lbu \$v0, 0(\$t0)' '$TMP/mips_global_arrays.c.mips.s' &&
     grep -q '29' '$TMP/mips_global_arrays.c.mips.s'"

check "mips_pointer_arith.c pointer arithmetic/indexed load" sh -c \
    "grep -q 'read_index' '$TMP/mips_pointer_arith.c.mips.s' &&
     grep -q 'sll \$v0, \$v0, 2' '$TMP/mips_pointer_arith.c.mips.s' &&
     grep -q 'addu \$v0, \$t0, \$v0' '$TMP/mips_pointer_arith.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$v0)' '$TMP/mips_pointer_arith.c.mips.s' &&
     grep -q '33' '$TMP/mips_pointer_arith.c.mips.s'"

check "mips_signed_unsigned_cmp.c signed/unsigned comparisons" sh -c \
    "grep -q 'slt \$v0, \$t0, \$v0' '$TMP/mips_signed_unsigned_cmp.c.mips.s' &&
     grep -q 'sltu \$v0, \$v0, \$t0' '$TMP/mips_signed_unsigned_cmp.c.mips.s' &&
     grep -q 'jal signed_lt' '$TMP/mips_signed_unsigned_cmp.c.mips.s' &&
     grep -q 'jal unsigned_gt' '$TMP/mips_signed_unsigned_cmp.c.mips.s'"

check "mips_short_circuit.c short-circuit branches" sh -c \
    "grep -q '_hits' '$TMP/mips_short_circuit.c.mips.s' &&
     grep -q 'jal bump' '$TMP/mips_short_circuit.c.mips.s' &&
     grep -q 'beq \$v0, \$zero' '$TMP/mips_short_circuit.c.mips.s' &&
     grep -q 'bne \$v0, \$zero' '$TMP/mips_short_circuit.c.mips.s'"

check "mips_nested_break_continue.c nested loop control" sh -c \
    "grep -q 'slt \$v0, \$t0, \$v0' '$TMP/mips_nested_break_continue.c.mips.s' &&
     grep -q 'bne \$v0, \$zero' '$TMP/mips_nested_break_continue.c.mips.s' &&
     grep -q 'b L3' '$TMP/mips_nested_break_continue.c.mips.s' &&
     grep -q 'b L4' '$TMP/mips_nested_break_continue.c.mips.s' &&
     grep -q '20' '$TMP/mips_nested_break_continue.c.mips.s'"

check "mips_funcptr_basic.c indirect call lowering" sh -c \
    "grep -q 'call_it' '$TMP/mips_funcptr_basic.c.mips.s' &&
     grep -q '\.globl add3' '$TMP/mips_funcptr_basic.c.mips.s' &&
     grep -q 'move \$t1, \$v0' '$TMP/mips_funcptr_basic.c.mips.s' &&
     grep -q 'jalr \$t1' '$TMP/mips_funcptr_basic.c.mips.s' &&
     grep -q '42' '$TMP/mips_funcptr_basic.c.mips.s'"


check "mips_div_mod_signed.c signed divide/modulo" sh -c \
    "grep -q 'divs' '$TMP/mips_div_mod_signed.c.mips.s' &&
     grep -q 'mods' '$TMP/mips_div_mod_signed.c.mips.s' &&
     grep -q 'div \$t0, \$v0' '$TMP/mips_div_mod_signed.c.mips.s' &&
     grep -q 'mflo \$v0' '$TMP/mips_div_mod_signed.c.mips.s' &&
     grep -q 'mfhi \$v0' '$TMP/mips_div_mod_signed.c.mips.s' &&
     grep -q 'jal divs' '$TMP/mips_div_mod_signed.c.mips.s' &&
     grep -q 'jal mods' '$TMP/mips_div_mod_signed.c.mips.s'"

check "mips_div_mod_unsigned.c unsigned divide/modulo" sh -c \
    "grep -q 'divu_test' '$TMP/mips_div_mod_unsigned.c.mips.s' &&
     grep -q 'modu_test' '$TMP/mips_div_mod_unsigned.c.mips.s' &&
     grep -q 'divu \$t0, \$v0' '$TMP/mips_div_mod_unsigned.c.mips.s' &&
     grep -q 'mflo \$v0' '$TMP/mips_div_mod_unsigned.c.mips.s' &&
     grep -q 'mfhi \$v0' '$TMP/mips_div_mod_unsigned.c.mips.s' &&
     grep -q 'jal divu_test' '$TMP/mips_div_mod_unsigned.c.mips.s' &&
     grep -q 'jal modu_test' '$TMP/mips_div_mod_unsigned.c.mips.s'"

check "mips_conditional_expr.c ternary branch/merge lowering" sh -c \
    "grep -q 'pick' '$TMP/mips_conditional_expr.c.mips.s' &&
     grep -q 'beq \$v0, \$zero' '$TMP/mips_conditional_expr.c.mips.s' &&
     grep -q 'b L' '$TMP/mips_conditional_expr.c.mips.s' &&
     grep -q 'jal pick' '$TMP/mips_conditional_expr.c.mips.s' &&
     grep -q '42' '$TMP/mips_conditional_expr.c.mips.s' &&
     grep -q '7' '$TMP/mips_conditional_expr.c.mips.s'"

check "mips_switch_basic.c switch compare/branch lowering" sh -c \
    "grep -q 'classify' '$TMP/mips_switch_basic.c.mips.s' &&
     grep -q 'bne \$v0, \$zero' '$TMP/mips_switch_basic.c.mips.s' &&
     grep -q 'b L5' '$TMP/mips_switch_basic.c.mips.s' &&
     grep -q '10' '$TMP/mips_switch_basic.c.mips.s' &&
     grep -q '20' '$TMP/mips_switch_basic.c.mips.s' &&
     grep -q '50' '$TMP/mips_switch_basic.c.mips.s' &&
     grep -q '99' '$TMP/mips_switch_basic.c.mips.s'"

check "mips_addr_of_local.c address-of local and dereference" sh -c \
    "grep -q 'addiu \$v0, \$fp, -4' '$TMP/mips_addr_of_local.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$v0)' '$TMP/mips_addr_of_local.c.mips.s' &&
     grep -q 'sw \$v0, 0(\$t0)' '$TMP/mips_addr_of_local.c.mips.s' &&
     grep -q '42' '$TMP/mips_addr_of_local.c.mips.s' &&
     grep -q '17' '$TMP/mips_addr_of_local.c.mips.s'"

check "mips_narrow_load_store.c narrow local load/store paths" sh -c \
    "grep -Eq 'sb|sh' '$TMP/mips_narrow_load_store.c.mips.s' &&
     grep -Eq 'lb|lbu|lh|lhu' '$TMP/mips_narrow_load_store.c.mips.s' &&
     grep -q -- '-3' '$TMP/mips_narrow_load_store.c.mips.s' &&
     grep -q '250' '$TMP/mips_narrow_load_store.c.mips.s' &&
     grep -q -- '-1234' '$TMP/mips_narrow_load_store.c.mips.s' &&
     grep -q '50000' '$TMP/mips_narrow_load_store.c.mips.s'"


check "mips_recursive_calls.c recursive call lowering" sh -c \
    "grep -q 'fact' '$TMP/mips_recursive_calls.c.mips.s' &&
     grep -q 'jal fact' '$TMP/mips_recursive_calls.c.mips.s' &&
     grep -q 'subu \$v0, \$t0, \$v0' '$TMP/mips_recursive_calls.c.mips.s' &&
     grep -q 'mul \$v0, \$t0, \$v0' '$TMP/mips_recursive_calls.c.mips.s' &&
     grep -q '120' '$TMP/mips_recursive_calls.c.mips.s'"

check "mips_nested_call_args.c nested calls and argument setup" sh -c \
    "grep -q 'add' '$TMP/mips_nested_call_args.c.mips.s' &&
     grep -q 'mul' '$TMP/mips_nested_call_args.c.mips.s' &&
     grep -q 'jal add' '$TMP/mips_nested_call_args.c.mips.s' &&
     grep -q 'jal mul' '$TMP/mips_nested_call_args.c.mips.s' &&
     grep -q 'prepare 2 call args scaffold' '$TMP/mips_nested_call_args.c.mips.s' &&
     grep -q '15' '$TMP/mips_nested_call_args.c.mips.s'"

check "mips_mutual_calls.c forward/mutual calls" sh -c \
    "grep -q 'even' '$TMP/mips_mutual_calls.c.mips.s' &&
     grep -q 'odd' '$TMP/mips_mutual_calls.c.mips.s' &&
     grep -q 'jal odd' '$TMP/mips_mutual_calls.c.mips.s' &&
     grep -q 'jal even' '$TMP/mips_mutual_calls.c.mips.s' &&
     grep -q 'subu \$v0, \$t0, \$v0' '$TMP/mips_mutual_calls.c.mips.s' &&
     grep -q '10' '$TMP/mips_mutual_calls.c.mips.s' &&
     grep -q '9' '$TMP/mips_mutual_calls.c.mips.s'"

check "mips_local_array_index.c local array indexed addressing" sh -c \
    "grep -q 'sw \$v0, -16(\$fp)' '$TMP/mips_local_array_index.c.mips.s' &&
     grep -q 'sw \$v0, -12(\$fp)' '$TMP/mips_local_array_index.c.mips.s' &&
     grep -q 'lw \$v0, -8(\$fp)' '$TMP/mips_local_array_index.c.mips.s' &&
     grep -q 'lw \$v0, -12(\$fp)' '$TMP/mips_local_array_index.c.mips.s' &&
     grep -q 'lw \$v0, -4(\$fp)' '$TMP/mips_local_array_index.c.mips.s' &&
     grep -q '7' '$TMP/mips_local_array_index.c.mips.s' &&
     grep -q '16' '$TMP/mips_local_array_index.c.mips.s'"

check "mips_pre_post_inc.c pre/post increment locals" sh -c \
    "grep -q 'addu \$v0, \$t0, \$v0' '$TMP/mips_pre_post_inc.c.mips.s' &&
     grep -q 'sw \$v0, -4(\$fp)' '$TMP/mips_pre_post_inc.c.mips.s' &&
     grep -q '10' '$TMP/mips_pre_post_inc.c.mips.s' &&
     grep -q '11' '$TMP/mips_pre_post_inc.c.mips.s' &&
     grep -q '12' '$TMP/mips_pre_post_inc.c.mips.s'"

check "mips_pointer_inc_store.c pointer dereference increment store" sh -c \
    "grep -q 'addiu \$v0, \$fp, -4' '$TMP/mips_pointer_inc_store.c.mips.s' &&
     grep -q 'move \$t1, \$v0' '$TMP/mips_pointer_inc_store.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$t1)' '$TMP/mips_pointer_inc_store.c.mips.s' &&
     grep -q 'sw \$v0, 0(\$t1)' '$TMP/mips_pointer_inc_store.c.mips.s' &&
     grep -q '40' '$TMP/mips_pointer_inc_store.c.mips.s' &&
     grep -q '41' '$TMP/mips_pointer_inc_store.c.mips.s' &&
     grep -q '42' '$TMP/mips_pointer_inc_store.c.mips.s'"


check "mips_local_struct_fields.c local struct field stack offsets" sh -c \
    "grep -q 'sw \$v0, -8(\$fp)' '$TMP/mips_local_struct_fields.c.mips.s' &&
     grep -q 'sw \$v0, -4(\$fp)' '$TMP/mips_local_struct_fields.c.mips.s' &&
     grep -q 'lw \$v0, -8(\$fp)' '$TMP/mips_local_struct_fields.c.mips.s' &&
     grep -q 'lw \$v0, -4(\$fp)' '$TMP/mips_local_struct_fields.c.mips.s' &&
     grep -q '17' '$TMP/mips_local_struct_fields.c.mips.s' &&
     grep -q '25' '$TMP/mips_local_struct_fields.c.mips.s' &&
     grep -q '42' '$TMP/mips_local_struct_fields.c.mips.s'"

check "mips_struct_ptr_fields.c struct pointer field access" sh -c \
    "grep -q 'get_value' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q 'set_next' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$v0)' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q 'sw \$v0, 4(\$t0)' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q 'jal set_next' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q 'jal get_value' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q '42' '$TMP/mips_struct_ptr_fields.c.mips.s' &&
     grep -q '17' '$TMP/mips_struct_ptr_fields.c.mips.s'"

check "mips_nested_struct_fields.c nested local struct field offsets" sh -c \
    "grep -q 'sw \$v0, -' '$TMP/mips_nested_struct_fields.c.mips.s' &&
     grep -q 'lw \$v0, -' '$TMP/mips_nested_struct_fields.c.mips.s' &&
     grep -q 'li \$v0, 5' '$TMP/mips_nested_struct_fields.c.mips.s' &&
     grep -q 'li \$v0, 10' '$TMP/mips_nested_struct_fields.c.mips.s' &&
     grep -q 'li \$v0, 20' '$TMP/mips_nested_struct_fields.c.mips.s' &&
     grep -q 'li \$v0, 7' '$TMP/mips_nested_struct_fields.c.mips.s' &&
     grep -q 'addu \$v0, \$t0, \$v0' '$TMP/mips_nested_struct_fields.c.mips.s'"

check "mips_array_of_structs.c aggregate data and indexed field loads" sh -c \
    "grep -q '_pairs:' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q '\.byte 1, 0, 0, 0, 2, 0, 0, 0, 10, 0, 0, 0, 20, 0, 0, 0' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q '\.byte 3, 0, 0, 0, 4, 0, 0, 0' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q 'la \$t0, pairs' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q 'addiu \$t0, \$t0, 8' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q 'addiu \$t0, \$t0, 12' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q 'addiu \$t0, \$t0, 16' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q 'addiu \$t0, \$t0, 20' '$TMP/mips_array_of_structs.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$t0)' '$TMP/mips_array_of_structs.c.mips.s'"

check "mips_global_struct_fields.c aggregate data and field loads" sh -c \
    "grep -q '_item' '$TMP/mips_global_struct_fields.c.mips.s' &&
     grep -q '\.byte 3, 0, 232, 3, 39, 0, 0, 0' '$TMP/mips_global_struct_fields.c.mips.s' &&
     grep -q 'la \$t0, item' '$TMP/mips_global_struct_fields.c.mips.s' &&
     grep -q 'lbu \$v0, 0(\$t0)' '$TMP/mips_global_struct_fields.c.mips.s' &&
     grep -q 'lhu \$v0, 0(\$t0)' '$TMP/mips_global_struct_fields.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$t0)' '$TMP/mips_global_struct_fields.c.mips.s' &&
     grep -q '1042' '$TMP/mips_global_struct_fields.c.mips.s'"

check "mips_string_global_ptr.c global string pointer data/load" sh -c \
    "grep -q '_msg' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q '\.ascii \"hello\"' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q '\.byte 0' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q '_p' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q '\.long _msg' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q 'la \$t0, p' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$t0)' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$v0)' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q '104' '$TMP/mips_string_global_ptr.c.mips.s' &&
     grep -q '111' '$TMP/mips_string_global_ptr.c.mips.s'"


check "mips_mixed_width_locals.c mixed-width local storage" sh -c \
    "grep -q 'sb \$v0, -4(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q 'sb \$v0, -8(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q 'sw \$v0, -12(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q 'sw \$v0, -16(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q 'sw \$v0, -20(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q 'lbu \$v0, -4(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q 'lbu \$v0, -8(\$fp)' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q -- '-5' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q '250' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q -- '-300' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q '60000' '$TMP/mips_mixed_width_locals.c.mips.s' &&
     grep -q '42' '$TMP/mips_mixed_width_locals.c.mips.s'"

check "mips_funcptr_reassign.c reassigned indirect calls" sh -c \
    "grep -q 'add1' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q 'add2' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q 'la \$v0, add1' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q 'la \$v0, add2' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q 'sw \$v0, -8(\$fp)' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q 'move \$t1, \$v0' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q 'jalr \$t1' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q '40' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q '41' '$TMP/mips_funcptr_reassign.c.mips.s' &&
     grep -q '42' '$TMP/mips_funcptr_reassign.c.mips.s'"

check "mips_funcptr_struct.c struct field indirect call" sh -c \
    "grep -q 'add5' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'add7' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'call_op' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'la \$v0, add5' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'la \$v0, add7' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'lw \$v0,' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'move \$t1, \$v0' '$TMP/mips_funcptr_struct.c.mips.s' &&
     grep -q 'jalr \$t1' '$TMP/mips_funcptr_struct.c.mips.s'"

check "mips_indirect_call_args.c multi-arg indirect call lowering" sh -c \
    "grep -q 'sum3' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'call_sum3' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q '\.globl sum3' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'li \$v0, 20' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'li \$v0, 10' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'addu \$v0, \$t0, \$v0' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'move \$t1, \$v0' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'prepare 3 call args scaffold' '$TMP/mips_indirect_call_args.c.mips.s' &&
     grep -q 'jalr \$t1' '$TMP/mips_indirect_call_args.c.mips.s'"

check "mips_funcptr_global.c global function pointer" sh -c \
    "grep -q '_global_fn' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q '\.long _add3' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q 'la \$t0, global_fn' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q 'lw \$v0, 0(\$t0)' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q 'move \$t1, \$v0' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q 'jalr \$t1' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q '39' '$TMP/mips_funcptr_global.c.mips.s' &&
     grep -q '42' '$TMP/mips_funcptr_global.c.mips.s'"

[ "$fail" -eq 0 ] || exit 1
echo "  CHECK mips_struct_arg_by_value.c struct argument pointer-lowered path"
ASM="$TMP/mips_struct_arg_by_value.c.mips.s"
grep -q '^\.globl sum_pair' "$ASM" || fail "mips_struct_arg_by_value.c missing sum_pair global"
grep -q '^sum_pair:' "$ASM" || fail "mips_struct_arg_by_value.c missing sum_pair label"
grep -q '^\.globl add_pair_and_int' "$ASM" || fail "mips_struct_arg_by_value.c missing add_pair_and_int global"
grep -q '^add_pair_and_int:' "$ASM" || fail "mips_struct_arg_by_value.c missing add_pair_and_int label"
grep -q 'sw \$a0, -8(\$fp)' "$ASM" || fail "mips_struct_arg_by_value.c did not save first argument"
grep -q 'sw \$a1, -16(\$fp)' "$ASM" || fail "mips_struct_arg_by_value.c did not save second argument"
grep -q 'sw \$a2, -24(\$fp)' "$ASM" || fail "mips_struct_arg_by_value.c did not save third argument"
grep -q 'addiu \$v0, \$fp, -8' "$ASM" || fail "mips_struct_arg_by_value.c did not pass local struct address"
grep -q 'lw \$v0, 4(\$v0)' "$ASM" || fail "mips_struct_arg_by_value.c missing second field load"
grep -q 'jal sum_pair' "$ASM" || fail "mips_struct_arg_by_value.c missing sum_pair call"
grep -q 'jal add_pair_and_int' "$ASM" || fail "mips_struct_arg_by_value.c missing add_pair_and_int call"
grep -q 'addu \$v0, \$t0, \$v0' "$ASM" || fail "mips_struct_arg_by_value.c missing integer add lowering"

echo "  CHECK mips_struct_arg_mixed_scalars.c mixed scalar/struct pointer-lowered args"
ASM="$TMP/mips_struct_arg_mixed_scalars.c.mips.s"
grep -q '^\.globl mix' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing mix global"
grep -q '^mix:' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing mix label"
grep -q '^\.globl main' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing main global"
grep -q '^main:' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing main label"
grep -q 'sw \$a0, -8(\$fp)' "$ASM" || fail "mips_struct_arg_mixed_scalars.c did not save first scalar arg"
grep -q 'sw \$a1, -16(\$fp)' "$ASM" || fail "mips_struct_arg_mixed_scalars.c did not save struct pointer arg"
grep -q 'sw \$a2, -24(\$fp)' "$ASM" || fail "mips_struct_arg_mixed_scalars.c did not save third scalar arg"
grep -q 'sw \$a3, -32(\$fp)' "$ASM" || fail "mips_struct_arg_mixed_scalars.c did not save fourth scalar arg"
grep -q 'lw \$v0, -16(\$fp)' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing struct pointer reload"
grep -q 'lw \$v0, 4(\$v0)' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing second struct field load"
grep -q '# prepare 4 call args scaffold' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing 4-arg call scaffold"
grep -q 'jal mix' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing mix call"
grep -q 'addu \$v0, \$t0, \$v0' "$ASM" || fail "mips_struct_arg_mixed_scalars.c missing integer add lowering"

echo "  CHECK mips_struct_return_small.c hidden return-buffer lowering"
ASM="$TMP/mips_struct_return_small.c.mips.s"
grep -q '^\.globl make_pair' "$ASM" || fail "mips_struct_return_small.c missing make_pair global"
grep -q '^make_pair:' "$ASM" || fail "mips_struct_return_small.c missing make_pair label"
grep -q '^\.globl main' "$ASM" || fail "mips_struct_return_small.c missing main global"
grep -q '^main:' "$ASM" || fail "mips_struct_return_small.c missing main label"
grep -q 'sw \$a0, -8(\$fp)' "$ASM" || fail "mips_struct_return_small.c did not save hidden return buffer arg"
grep -q 'sw \$a1, -16(\$fp)' "$ASM" || fail "mips_struct_return_small.c did not save first value arg"
grep -q 'sw \$a2, -24(\$fp)' "$ASM" || fail "mips_struct_return_small.c did not save second value arg"
grep -q 'addiu \$v0, \$fp, -8' "$ASM" || fail "mips_struct_return_small.c did not pass local return buffer"
grep -q '# prepare 3 call args scaffold' "$ASM" || fail "mips_struct_return_small.c missing 3-arg call scaffold"
grep -q 'jal make_pair' "$ASM" || fail "mips_struct_return_small.c missing make_pair call"
grep -q 'move \$t1, \$v0' "$ASM" || fail "mips_struct_return_small.c missing return-buffer destination setup"
grep -q 'sw \$t0, 0(\$t1)' "$ASM" || fail "mips_struct_return_small.c missing first return-buffer field store"
grep -q 'sw \$t0, 4(\$t1)' "$ASM" || fail "mips_struct_return_small.c missing second return-buffer field store"
grep -q 'lw \$v0, -8(\$fp)' "$ASM" || fail "mips_struct_return_small.c missing reload of returned first field"
grep -q 'lw \$v0, -4(\$fp)' "$ASM" || fail "mips_struct_return_small.c missing reload of returned second field"
grep -q 'addu \$v0, \$t0, \$v0' "$ASM" || fail "mips_struct_return_small.c missing final add lowering"

echo "  CHECK mips_struct_return_medium.c hidden return-buffer lowering"
ASM="$TMP/mips_struct_return_medium.c.mips.s"
grep -q '^\.globl make_quad' "$ASM" || fail "mips_struct_return_medium.c missing make_quad global"
grep -q '^make_quad:' "$ASM" || fail "mips_struct_return_medium.c missing make_quad label"
grep -q '^\.globl main' "$ASM" || fail "mips_struct_return_medium.c missing main global"
grep -q '^main:' "$ASM" || fail "mips_struct_return_medium.c missing main label"
grep -q 'sw \$a0, -8(\$fp)' "$ASM" || fail "mips_struct_return_medium.c did not save hidden return buffer arg"
grep -q 'sw \$a1, -16(\$fp)' "$ASM" || fail "mips_struct_return_medium.c did not save first value arg"
grep -q 'sw \$a2, -24(\$fp)' "$ASM" || fail "mips_struct_return_medium.c did not save second value arg"
grep -q 'sw \$a3, -32(\$fp)' "$ASM" || fail "mips_struct_return_medium.c did not save third value arg"
grep -q '# stack parameter 4 -> -40(\$fp) scaffold' "$ASM" || fail "mips_struct_return_medium.c missing stack arg scaffold"
grep -q 'addiu \$v0, \$fp, -16' "$ASM" || fail "mips_struct_return_medium.c did not pass local return buffer"
grep -q '# prepare 5 call args scaffold' "$ASM" || fail "mips_struct_return_medium.c missing 5-arg call scaffold"
grep -q 'jal make_quad' "$ASM" || fail "mips_struct_return_medium.c missing make_quad call"
grep -q 'move \$t1, \$v0' "$ASM" || fail "mips_struct_return_medium.c missing return-buffer destination setup"
grep -q 'sw \$t0, 0(\$t1)' "$ASM" || fail "mips_struct_return_medium.c missing returned field store 0"
grep -q 'sw \$t0, 4(\$t1)' "$ASM" || fail "mips_struct_return_medium.c missing returned field store 4"
grep -q 'sw \$t0, 8(\$t1)' "$ASM" || fail "mips_struct_return_medium.c missing returned field store 8"
grep -q 'sw \$t0, 12(\$t1)' "$ASM" || fail "mips_struct_return_medium.c missing returned field store 12"
grep -q 'lw \$v0, -16(\$fp)' "$ASM" || fail "mips_struct_return_medium.c missing reload of returned field 0"
grep -q 'lw \$v0, -12(\$fp)' "$ASM" || fail "mips_struct_return_medium.c missing reload of returned field 1"
grep -q 'lw \$v0, -8(\$fp)' "$ASM" || fail "mips_struct_return_medium.c missing reload of returned field 2"
grep -q 'lw \$v0, -4(\$fp)' "$ASM" || fail "mips_struct_return_medium.c missing reload of returned field 3"
grep -q 'addu \$v0, \$t0, \$v0' "$ASM" || fail "mips_struct_return_medium.c missing final add lowering"

echo "  CHECK mips_varargs_sum.c varargs scaffold with MIPS va_base helper"
ASM="$TMP/mips_varargs_sum.c.mips.s"
grep -q '^__tcc_va_base:' "$ASM" || fail "mips_varargs_sum.c missing __tcc_va_base helper"
grep -q '^\.globl sum_ints' "$ASM" || fail "mips_varargs_sum.c missing sum_ints global"
grep -q '^sum_ints:' "$ASM" || fail "mips_varargs_sum.c missing sum_ints label"
grep -q '^\.globl main' "$ASM" || fail "mips_varargs_sum.c missing main global"
grep -q '^main:' "$ASM" || fail "mips_varargs_sum.c missing main label"
grep -q 'jal __tcc_va_base' "$ASM" || fail "mips_varargs_sum.c missing va_base call"
grep -q 'sw \$a0, -8(\$fp)' "$ASM" || fail "mips_varargs_sum.c did not save fixed count arg"
grep -q 'sw \$v0, -16(\$fp)' "$ASM" || fail "mips_varargs_sum.c missing va_list local save"
grep -q 'sw \$v0, -20(\$fp)' "$ASM" || fail "mips_varargs_sum.c missing sum local save"
grep -q 'sw \$v0, -24(\$fp)' "$ASM" || fail "mips_varargs_sum.c missing loop index local save"
grep -q 'slt \$v0, \$t0, \$v0' "$ASM" || fail "mips_varargs_sum.c missing loop compare"
grep -q 'xori \$v0, \$v0, 1' "$ASM" || fail "mips_varargs_sum.c missing loop compare inversion"
grep -q 'bne \$v0, \$zero, L3' "$ASM" || fail "mips_varargs_sum.c missing loop exit branch"
grep -q 'li \$v0, 8' "$ASM" || fail "mips_varargs_sum.c missing va_arg slot advance"
grep -q 'addu \$v0, \$t0, \$v0' "$ASM" || fail "mips_varargs_sum.c missing add lowering"
grep -q 'lw \$v0, 0(\$v0)' "$ASM" || fail "mips_varargs_sum.c missing vararg load"
grep -q '# prepare 5 call args scaffold' "$ASM" || fail "mips_varargs_sum.c missing 5-arg call scaffold"
grep -q 'jal sum_ints' "$ASM" || fail "mips_varargs_sum.c missing sum_ints call"
grep -q 'lw \$v0, 0(\$fp)' "$ASM" || fail "mips_varargs_sum.c missing MIPS va_base caller-fp load"
grep -q 'addiu \$v0, \$v0, 16' "$ASM" || fail "mips_varargs_sum.c missing MIPS va_base first-vararg adjustment"
if grep -q 'ldr x0, \[x29\]' "$ASM"; then echo "mips_varargs_sum.c still emits ARM64 va_base helper"; exit 1; fi
if grep -q 'add x0, x0, #16' "$ASM"; then echo "mips_varargs_sum.c still emits ARM64 va_base helper"; exit 1; fi

echo "  CHECK mips_varargs_mixed.c mixed varargs scaffold with MIPS va_base helper"
ASM="$TMP/mips_varargs_mixed.c.mips.s"
grep -q '^__tcc_va_base:' "$ASM" || fail "mips_varargs_mixed.c missing __tcc_va_base helper"
grep -q '^\.globl mix_args' "$ASM" || fail "mips_varargs_mixed.c missing mix_args global"
grep -q '^mix_args:' "$ASM" || fail "mips_varargs_mixed.c missing mix_args label"
grep -q '^\.globl main' "$ASM" || fail "mips_varargs_mixed.c missing main global"
grep -q '^main:' "$ASM" || fail "mips_varargs_mixed.c missing main label"
grep -q 'jal __tcc_va_base' "$ASM" || fail "mips_varargs_mixed.c missing va_base call"
grep -q 'sw \$a0, -8(\$fp)' "$ASM" || fail "mips_varargs_mixed.c did not save fixed argument"
grep -q 'sw \$v0, -16(\$fp)' "$ASM" || fail "mips_varargs_mixed.c missing va_list local save"
grep -q 'li \$v0, 8' "$ASM" || fail "mips_varargs_mixed.c missing va_arg slot advance"
grep -q 'lw \$v0, 0(\$v0)' "$ASM" || fail "mips_varargs_mixed.c missing vararg load"
grep -q 'sw \$v0, -20(\$fp)' "$ASM" || fail "mips_varargs_mixed.c missing first vararg local save"
grep -q 'sw \$v0, -28(\$fp)' "$ASM" || fail "mips_varargs_mixed.c missing second vararg local save"
grep -q 'sw \$v0, -32(\$fp)' "$ASM" || fail "mips_varargs_mixed.c missing third vararg local save"
grep -q '# prepare 4 call args scaffold' "$ASM" || fail "mips_varargs_mixed.c missing 4-arg call scaffold"
grep -q 'jal mix_args' "$ASM" || fail "mips_varargs_mixed.c missing mix_args call"
grep -q 'addu \$v0, \$t0, \$v0' "$ASM" || fail "mips_varargs_mixed.c missing add lowering"
grep -q 'lw \$v0, 0(\$fp)' "$ASM" || fail "mips_varargs_mixed.c missing MIPS va_base caller-fp load"
grep -q 'addiu \$v0, \$v0, 16' "$ASM" || fail "mips_varargs_mixed.c missing MIPS va_base first-vararg adjustment"
if grep -q 'ldr x0, \[x29\]' "$ASM"; then echo "mips_varargs_mixed.c still emits ARM64 va_base helper"; exit 1; fi
if grep -q 'add x0, x0, #16' "$ASM"; then echo "mips_varargs_mixed.c still emits ARM64 va_base helper"; exit 1; fi

echo "  CHECK mips_i64_mul_div_mod.c i64 helper-call lowering scaffold"
ASM="$TMP/mips_i64_mul_div_mod.c.mips.s"
grep -q '^\.globl mul64' "$ASM" || fail "mips_i64_mul_div_mod.c missing mul64 global"
grep -q '^mul64:' "$ASM" || fail "mips_i64_mul_div_mod.c missing mul64 label"
grep -q '^\.globl div64' "$ASM" || fail "mips_i64_mul_div_mod.c missing div64 global"
grep -q '^div64:' "$ASM" || fail "mips_i64_mul_div_mod.c missing div64 label"
grep -q '^\.globl mod64' "$ASM" || fail "mips_i64_mul_div_mod.c missing mod64 global"
grep -q '^mod64:' "$ASM" || fail "mips_i64_mul_div_mod.c missing mod64 label"
grep -q '^\.globl main' "$ASM" || fail "mips_i64_mul_div_mod.c missing main global"
grep -q '^main:' "$ASM" || fail "mips_i64_mul_div_mod.c missing main label"
grep -q 'jal __tcc_mips_i64_mul' "$ASM" || fail "mips_i64_mul_div_mod.c missing i64 mul helper call"
grep -q 'jal __tcc_mips_i64_div' "$ASM" || fail "mips_i64_mul_div_mod.c missing i64 div helper call"
grep -q 'jal __tcc_mips_i64_mod' "$ASM" || fail "mips_i64_mul_div_mod.c missing i64 mod helper call"
if grep -q 'mul \$v0, \$t0, \$v0' "$ASM"; then echo "mips_i64_mul_div_mod.c still uses low-word multiply placeholder"; exit 1; fi
grep -q 'li \$v0, 1000' "$ASM" || fail "mips_i64_mul_div_mod.c missing first small i64 probe constant"
grep -q 'li \$v0, 3' "$ASM" || fail "mips_i64_mul_div_mod.c missing second small i64 probe constant"
if grep -q 'li \$v0, 10000000000' "$ASM"; then echo "mips_i64_mul_div_mod.c still emits oversized i64 immediate placeholder"; exit 1; fi
grep -q '# prepare 2 call args scaffold' "$ASM" || fail "mips_i64_mul_div_mod.c missing 2-arg call scaffold"
grep -q 'jal mul64' "$ASM" || fail "mips_i64_mul_div_mod.c missing mul64 call"
grep -q 'jal div64' "$ASM" || fail "mips_i64_mul_div_mod.c missing div64 call"
grep -q 'jal mod64' "$ASM" || fail "mips_i64_mul_div_mod.c missing mod64 call"
grep -q 'xor \$v0, \$t0, \$v0' "$ASM" || fail "mips_i64_mul_div_mod.c missing equality compare xor scaffold"
grep -q 'sltu \$v0, \$zero, \$v0' "$ASM" || fail "mips_i64_mul_div_mod.c missing equality result lowering"

[ "$fail" -eq 0 ] || exit 1
echo "MIPS smoke test OK"

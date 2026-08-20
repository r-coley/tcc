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
        --flags) shift 2 ;; # accepted for interface parity with other smoke scripts
        *)
            echo "unknown option: $1" >&2
            exit 2
            ;;
    esac
done

if [ -z "${CC}" ]; then
    CC=${CC:-build/tcc_stage0}
fi

if [ -z "${TMP}" ]; then
    TMP=${TMP:-build/tmp}
fi

mkdir -p "$TMP"

echo "m68k assembly-generation smoke test:"

compile() {
    src="$1"
    base="$(basename "$src")"
    out="$TMP/$base.m68k.s"

    if "$CC" -S -target=m68k -mcpu=68000 "$src" -o "$out" >/dev/null 2>"$TMP/$base.m68k.err"; then
        echo "  PASS $base"
    else
        echo "  FAIL $base"
        cat "$TMP/$base.m68k.err"
        exit 1
    fi
}

check() {
    name="$1"
    shift

    printf "  CHECK %s\n" "$name"
    "$@"
}

compile "$TEST_DIR/backend/m68k_arith.c"
compile "$TEST_DIR/backend/m68k_global.c"
compile "$TEST_DIR/backend/m68k_local.c"
compile "$TEST_DIR/backend/m68k_call_probe.c"
compile "$TEST_DIR/backend/m68k_sub.c"
compile "$TEST_DIR/backend/m68k_bitwise.c"
compile "$TEST_DIR/backend/m68k_compare.c"
compile "$TEST_DIR/backend/m68k_branch_loop.c"
compile "$TEST_DIR/backend/m68k_addr_of_local.c"
compile "$TEST_DIR/backend/m68k_pointer_load_store.c"
compile "$TEST_DIR/backend/m68k_global_arrays.c"
compile "$TEST_DIR/backend/m68k_struct_ptr_fields.c"
compile "$TEST_DIR/backend/m68k_local_array_index.c"
compile "$TEST_DIR/backend/m68k_pre_post_inc.c"
compile "$TEST_DIR/backend/m68k_pointer_inc_store.c"
compile "$TEST_DIR/backend/m68k_mixed_width_locals.c"
compile "$TEST_DIR/backend/m68k_funcptr_basic.c"
compile "$TEST_DIR/backend/m68k_signed_unsigned_cmp.c"
compile "$TEST_DIR/backend/m68k_div_mod_signed.c"
compile "$TEST_DIR/backend/m68k_div_mod_unsigned.c"
compile "$TEST_DIR/backend/m68k_conditional_expr.c"
compile "$TEST_DIR/backend/m68k_switch_basic.c"
compile "$TEST_DIR/backend/m68k_short_circuit.c"
compile "$TEST_DIR/backend/m68k_nested_break_continue.c"
compile "$TEST_DIR/backend/m68k_recursive_calls.c"
compile "$TEST_DIR/backend/m68k_nested_call_args.c"
compile "$TEST_DIR/backend/m68k_mutual_calls.c"
compile "$TEST_DIR/backend/m68k_funcptr_reassign.c"
compile "$TEST_DIR/backend/m68k_funcptr_global.c"
compile "$TEST_DIR/backend/m68k_local_struct_fields.c"
compile "$TEST_DIR/backend/m68k_nested_struct_fields.c"
compile "$TEST_DIR/backend/m68k_array_of_structs.c"
compile "$TEST_DIR/backend/m68k_global_struct_fields.c"
compile "$TEST_DIR/backend/m68k_string_global_ptr.c"
compile "$TEST_DIR/backend/m68k_funcptr_struct.c"
compile "$TEST_DIR/backend/m68k_indirect_call_args.c"
compile "$TEST_DIR/backend/m68k_narrow_struct_fields.c"
compile "$TEST_DIR/backend/m68k_static_locals.c"

check "m68k_arith.c constant return" sh -c \
    "grep -q '_main' '$TMP/m68k_arith.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_arith.c.m68k.s' &&
     grep -q 'rts' '$TMP/m68k_arith.c.m68k.s'"

check "m68k_global.c global data/load" sh -c \
    "grep -q '_x' '$TMP/m68k_global.c.m68k.s' &&
     grep -q '.long 42' '$TMP/m68k_global.c.m68k.s' &&
     grep -q 'move.l _x,d0' '$TMP/m68k_global.c.m68k.s'"

check "m68k_local.c local stack storage" sh -c \
    "grep -q 'sub.l #4,sp' '$TMP/m68k_local.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_local.c.m68k.s' &&
     grep -q 'move.l d0,-4(a6)' '$TMP/m68k_local.c.m68k.s' &&
     grep -q 'move.l -4(a6),d0' '$TMP/m68k_local.c.m68k.s'"

check "m68k_call_probe.c call and binary add lowering" sh -c \
    "grep -q '_add' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'move.l 8(a6),d0' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'move.l 12(a6),d0' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'move.l (sp)+,d0' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'move.l (sp)+,d1' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'add.l d1,d0' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'jsr _add' '$TMP/m68k_call_probe.c.m68k.s' &&
     grep -q 'add.l #8,sp' '$TMP/m68k_call_probe.c.m68k.s'"


check "m68k_sub.c folded subtraction return" sh -c \
    "grep -q '_main' '$TMP/m68k_sub.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_sub.c.m68k.s' &&
     grep -q 'rts' '$TMP/m68k_sub.c.m68k.s'"

check "m68k_bitwise.c folded bitwise return" sh -c \
    "grep -q '_main' '$TMP/m68k_bitwise.c.m68k.s' &&
     grep -q 'moveq #11,d0' '$TMP/m68k_bitwise.c.m68k.s' &&
     grep -q 'rts' '$TMP/m68k_bitwise.c.m68k.s'"

check "m68k_compare.c folded compare branch" sh -c \
    "grep -q 'moveq #1,d0' '$TMP/m68k_compare.c.m68k.s' &&
     grep -q 'tst.l d0' '$TMP/m68k_compare.c.m68k.s' &&
     grep -q 'beq .L1' '$TMP/m68k_compare.c.m68k.s' &&
     grep -q 'moveq #0,d0' '$TMP/m68k_compare.c.m68k.s'"

check "m68k_branch_loop.c loop and compare lowering" sh -c \
    "grep -q 'sub.l #8,sp' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'move.l d0,-4(a6)' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'move.l d0,-8(a6)' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'cmp.l d0,d1' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'slt d0' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'sne d0' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'add.l d1,d0' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'bra .L1' '$TMP/m68k_branch_loop.c.m68k.s' &&
     grep -q 'moveq #10,d0' '$TMP/m68k_branch_loop.c.m68k.s'"


check "m68k_addr_of_local.c address-of local and dereference" sh -c \
    "grep -q 'sub.l #12,sp' '$TMP/m68k_addr_of_local.c.m68k.s' &&
     grep -q 'lea -4(a6),a0' '$TMP/m68k_addr_of_local.c.m68k.s' &&
     grep -q 'move.l a0,d0' '$TMP/m68k_addr_of_local.c.m68k.s' &&
     grep -q 'move.l d0,-12(a6)' '$TMP/m68k_addr_of_local.c.m68k.s' &&
     grep -q 'move.l d0,a0' '$TMP/m68k_addr_of_local.c.m68k.s' &&
     grep -q 'move.l (a0),d0' '$TMP/m68k_addr_of_local.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_addr_of_local.c.m68k.s'"

check "m68k_pointer_load_store.c pointer load/store" sh -c \
    "grep -q 'lea -4(a6),a0' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'move.l d0,-12(a6)' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'move.l (a0),d0' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'add.l d1,d0' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'move.l d1,a0' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'move.l d0,(a0)' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'moveq #40,d0' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'moveq #2,d0' '$TMP/m68k_pointer_load_store.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_pointer_load_store.c.m68k.s'"

check "m68k_global_arrays.c global array indexed loads" sh -c \
    "grep -q '_values' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q '.long 10' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q '.long 20' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q '.long 12' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q 'asl.l #2,d0' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q 'lea _values,a0' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q 'adda.l d0,a0' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q 'move.l (a0),d0' '$TMP/m68k_global_arrays.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_global_arrays.c.m68k.s'"


check "m68k_struct_ptr_fields.c struct pointer field access" sh -c \
    "grep -q 'sub.l #16,sp' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'move.l d0,-8(a6)' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'move.l d0,-4(a6)' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'lea -8(a6),a0' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'move.l d0,-16(a6)' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'add.l #4,d0' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'move.l d1,a0' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'move.l d0,(a0)' '$TMP/m68k_struct_ptr_fields.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_struct_ptr_fields.c.m68k.s'"


check "m68k_local_array_index.c local array indexed addressing" sh -c \
    "grep -q 'sub.l #12,sp' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'move.l d0,a1' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'move.l d1,d0' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'asl.l #2,d0' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'lea -12(a6),a0' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'adda.l d0,a0' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'move.l a1,(a0)' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'move.l (a0),d0' '$TMP/m68k_local_array_index.c.m68k.s' &&
     grep -q 'moveq #42,d0' '$TMP/m68k_local_array_index.c.m68k.s'"


check "m68k_pre_post_inc.c pre/post increment locals" sh -c \
    "grep -q 'sub.l #8,sp' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'move.l d0,-4(a6)' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'move.l d0,-8(a6)' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'moveq #10,d0' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'moveq #11,d0' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'moveq #12,d0' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'moveq #4,d0' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'add.l d1,d0' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q '.L8:' '$TMP/m68k_pre_post_inc.c.m68k.s' &&
     grep -q 'moveq #0,d0' '$TMP/m68k_pre_post_inc.c.m68k.s'"


echo "  CHECK m68k_mixed_width_locals.c mixed-width local storage"
grep -q 'move.b d0,-4(a6)' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'move.w d0,-12(a6)' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'move.l d0,-16(a6)' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'move.b -4(a6),d0' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'ext.w d0' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'move.w -12(a6),d0' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'ext.l d0' "$TMP/m68k_mixed_width_locals.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_mixed_width_locals.c.m68k.s"


echo "  CHECK m68k_funcptr_basic.c function pointer indirect call"
grep -q 'lea _add1,a0' "$TMP/m68k_funcptr_basic.c.m68k.s"
grep -q 'move.l a0,d0' "$TMP/m68k_funcptr_basic.c.m68k.s"
grep -q 'move.l d0,-8(a6)' "$TMP/m68k_funcptr_basic.c.m68k.s"
grep -q 'move.l -8(a6),d0' "$TMP/m68k_funcptr_basic.c.m68k.s"
grep -q 'move.l d0,a0' "$TMP/m68k_funcptr_basic.c.m68k.s"
grep -q 'jsr (a0)' "$TMP/m68k_funcptr_basic.c.m68k.s"
grep -q 'add.l #4,sp' "$TMP/m68k_funcptr_basic.c.m68k.s"


echo "  CHECK m68k_signed_unsigned_cmp.c signed/unsigned comparisons"
grep -q 'moveq #-1,d0' "$TMP/m68k_signed_unsigned_cmp.c.m68k.s"
grep -q 'moveq #1,d0' "$TMP/m68k_signed_unsigned_cmp.c.m68k.s"
grep -q 'slt d0' "$TMP/m68k_signed_unsigned_cmp.c.m68k.s"
grep -q 'shi d0' "$TMP/m68k_signed_unsigned_cmp.c.m68k.s"
grep -q 'moveq #0,d0' "$TMP/m68k_signed_unsigned_cmp.c.m68k.s"


echo "  CHECK m68k_div_mod_signed.c signed div/mod lowering"
grep -q 'moveq #43,d0' "$TMP/m68k_div_mod_signed.c.m68k.s"
grep -q 'moveq #10,d0' "$TMP/m68k_div_mod_signed.c.m68k.s"
grep -q 'divs.w d2,d0' "$TMP/m68k_div_mod_signed.c.m68k.s"
grep -q 'swap d0' "$TMP/m68k_div_mod_signed.c.m68k.s"
grep -q 'moveq #4,d0' "$TMP/m68k_div_mod_signed.c.m68k.s"
grep -q 'moveq #3,d0' "$TMP/m68k_div_mod_signed.c.m68k.s"


echo "  CHECK m68k_div_mod_unsigned.c unsigned div/mod lowering"
grep -q 'moveq #43,d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"
grep -q 'moveq #10,d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"
grep -q 'divu.w d2,d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"
grep -q 'and.l #65535,d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"
grep -q 'swap d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"
grep -q 'moveq #4,d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"
grep -q 'moveq #3,d0' "$TMP/m68k_div_mod_unsigned.c.m68k.s"


echo "  CHECK m68k_conditional_expr.c ternary branch/merge lowering"
grep -q 'tst.l d0' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q 'beq .L1' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q 'bra .L2' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q '.L1:' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q '.L2:' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q 'beq .L5' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q 'bra .L6' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q '.L5:' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q '.L6:' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_conditional_expr.c.m68k.s"
grep -q 'moveq #38,d0' "$TMP/m68k_conditional_expr.c.m68k.s"


echo "  CHECK m68k_switch_basic.c switch compare/branch lowering"
grep -q 'moveq #1,d0' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'moveq #2,d0' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'moveq #3,d0' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'cmp.l d0,d1' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'seq d0' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'bne .L2' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'bne .L3' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'bne .L4' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'bra .L5' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_switch_basic.c.m68k.s"
grep -q 'moveq #99,d0' "$TMP/m68k_switch_basic.c.m68k.s"


echo "  CHECK m68k_short_circuit.c short-circuit branches"
grep -q 'tst.l d0' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q 'beq .L3' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q 'seq d0' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q 'bra .L4' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q '.L3:' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q '.L4:' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q 'moveq #0,d0' "$TMP/m68k_short_circuit.c.m68k.s"
grep -q 'moveq #1,d0' "$TMP/m68k_short_circuit.c.m68k.s"


echo "  CHECK m68k_nested_break_continue.c nested loop control"
grep -q '.L1:' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q '.L4:' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q 'bra .L5' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q 'bra .L6' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q '.L5:' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q '.L6:' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q 'bra .L1' "$TMP/m68k_nested_break_continue.c.m68k.s"
grep -q 'moveq #9,d0' "$TMP/m68k_nested_break_continue.c.m68k.s"


echo "  CHECK m68k_recursive_calls.c recursive call lowering"
grep -q '.globl _fact' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q '_fact:' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q 'move.l 8(a6),d0' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q 'sle d0' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q 'jsr _fact' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q 'add.l #4,sp' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q 'muls.w d1,d0' "$TMP/m68k_recursive_calls.c.m68k.s"
grep -q 'moveq #120,d0' "$TMP/m68k_recursive_calls.c.m68k.s"


echo "  CHECK m68k_nested_call_args.c nested calls and argument cleanup"
grep -q '.globl _add' "$TMP/m68k_nested_call_args.c.m68k.s"
grep -q '.globl _mul' "$TMP/m68k_nested_call_args.c.m68k.s"
grep -q 'jsr _add' "$TMP/m68k_nested_call_args.c.m68k.s"
grep -q 'jsr _mul' "$TMP/m68k_nested_call_args.c.m68k.s"
grep -q 'add.l #8,sp' "$TMP/m68k_nested_call_args.c.m68k.s"
grep -q 'muls.w d1,d0' "$TMP/m68k_nested_call_args.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_nested_call_args.c.m68k.s"


echo "  CHECK m68k_mutual_calls.c forward/mutual calls"
grep -q '.globl _even' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q '_even:' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q '.globl _odd' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q '_odd:' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q 'jsr _odd' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q 'jsr _even' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q 'add.l #4,sp' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q 'moveq #10,d0' "$TMP/m68k_mutual_calls.c.m68k.s"
grep -q 'moveq #9,d0' "$TMP/m68k_mutual_calls.c.m68k.s"


echo "  CHECK m68k_funcptr_reassign.c reassigned indirect calls"
grep -q '.globl _add1' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q '.globl _add2' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'lea _add1,a0' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'lea _add2,a0' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'move.l d0,-8(a6)' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'move.l d0,a0' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'jsr (a0)' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'add.l #4,sp' "$TMP/m68k_funcptr_reassign.c.m68k.s"
grep -q 'moveq #43,d0' "$TMP/m68k_funcptr_reassign.c.m68k.s"


echo "  CHECK m68k_funcptr_global.c global function pointer"
grep -q '.global _gfp' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q '_gfp:' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q '.long _add3' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q '.globl _add3' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q 'move.l _gfp,d0' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q 'move.l d0,a0' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q 'jsr (a0)' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q 'add.l #4,sp' "$TMP/m68k_funcptr_global.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_funcptr_global.c.m68k.s"


echo "  CHECK m68k_local_struct_fields.c local struct field stack offsets"
grep -q 'sub.l #8,sp' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'moveq #19,d0' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'move.l d0,-8(a6)' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'moveq #23,d0' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'move.l d0,-4(a6)' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'move.l -8(a6),d0' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'move.l -4(a6),d0' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'add.l d1,d0' "$TMP/m68k_local_struct_fields.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_local_struct_fields.c.m68k.s"


echo "  CHECK m68k_nested_struct_fields.c nested local struct field offsets"
grep -q 'sub.l #16,sp' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'moveq #10,d0' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'move.l d0,-16(a6)' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'moveq #11,d0' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'move.l d0,-12(a6)' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'moveq #12,d0' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'move.l d0,-8(a6)' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'moveq #9,d0' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'move.l d0,-4(a6)' "$TMP/m68k_nested_struct_fields.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_nested_struct_fields.c.m68k.s"


echo "  CHECK m68k_array_of_structs.c aggregate data and indexed field loads"
grep -q '.global _items' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q '_items:' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q '.byte 10, 0, 0, 0, 11, 0, 0, 0, 20, 0, 0, 0, 22, 0, 0, 0' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q 'lea _items,a0' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q 'moveq #0,d0' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q 'moveq #4,d0' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q 'moveq #12,d0' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q 'move.l (a0),d0' "$TMP/m68k_array_of_structs.c.m68k.s"
grep -q 'moveq #43,d0' "$TMP/m68k_array_of_structs.c.m68k.s"


echo "  CHECK m68k_global_struct_fields.c aggregate data and field loads"
grep -q '.global _gp' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q '_gp:' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q '.byte 19, 0, 0, 0, 23, 0, 0, 0' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q 'lea _gp,a0' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q 'moveq #0,d0' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q 'moveq #4,d0' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q 'move.l (a0),d0' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q 'add.l d1,d0' "$TMP/m68k_global_struct_fields.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_global_struct_fields.c.m68k.s"


echo "  CHECK m68k_string_global_ptr.c global string pointer data/load"
grep -q '.Lstr1:' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q '.byte 104' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q '.byte 101' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q '.global _msg' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q '_msg:' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q '.long .Lstr1' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q 'move.l _msg,d0' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q 'move.b (a0),d0' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q 'ext.w d0' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q 'ext.l d0' "$TMP/m68k_string_global_ptr.c.m68k.s"
grep -q 'moveq #101,d0' "$TMP/m68k_string_global_ptr.c.m68k.s"



echo "  CHECK m68k_funcptr_struct.c struct field indirect call"
grep -q '.globl _add5' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q '.globl _add7' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q '.globl _call_op' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q 'lea _add5,a0' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q 'lea _add7,a0' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q 'move.l (a0),d0' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q 'move.l d0,a0' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q 'jsr (a0)' "$TMP/m68k_funcptr_struct.c.m68k.s"
grep -q 'add.l #4,sp' "$TMP/m68k_funcptr_struct.c.m68k.s"

echo "  CHECK m68k_indirect_call_args.c multi-arg indirect call cleanup"
grep -q '.globl _sum3' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q '_sum3:' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'lea _sum3,a0' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'moveq #12,d0' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'moveq #20,d0' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'moveq #10,d0' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'move.l d0,a0' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'jsr (a0)' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'add.l #12,sp' "$TMP/m68k_indirect_call_args.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_indirect_call_args.c.m68k.s"


echo "  CHECK m68k_narrow_struct_fields.c narrow local struct fields"
grep -q 'sub.l #8,sp' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'move.b d0,-8(a6)' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'move.w d0,-6(a6)' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'move.l d0,-4(a6)' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'move.b -8(a6),d0' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'move.w -6(a6),d0' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'ext.w d0' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'ext.l d0' "$TMP/m68k_narrow_struct_fields.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_narrow_struct_fields.c.m68k.s"


echo "  CHECK m68k_static_locals.c static storage load/store"
grep -q '.global ___static_next_counter' "$TMP/m68k_static_locals.c.m68k.s"
grep -q '___static_next_counter:' "$TMP/m68k_static_locals.c.m68k.s"
grep -q '.long 40' "$TMP/m68k_static_locals.c.m68k.s"
grep -q '.globl _next' "$TMP/m68k_static_locals.c.m68k.s"
grep -q 'move.l ___static_next_counter,d0' "$TMP/m68k_static_locals.c.m68k.s"
grep -q 'move.l d0,___static_next_counter' "$TMP/m68k_static_locals.c.m68k.s"
grep -q 'jsr _next' "$TMP/m68k_static_locals.c.m68k.s"
grep -q 'moveq #41,d0' "$TMP/m68k_static_locals.c.m68k.s"
grep -q 'moveq #42,d0' "$TMP/m68k_static_locals.c.m68k.s"

echo "m68k smoke test OK"

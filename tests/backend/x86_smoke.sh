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
        --flags) shift 2 ;; # accepted for future use
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
    if "$CC" -S -target=x86 "$src" -o "$TMP/$base.x86.s" >/dev/null 2>"$TMP/$base.x86.err" && [ -s "$TMP/$base.x86.s" ]; then
        echo "  PASS $base"
    else
        echo "  FAIL $base"
        cat "$TMP/$base.x86.err"
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

echo "x86 assembly-generation smoke test:"

for src in \
    "$TEST_DIR/core/test001.c" \
    "$TEST_DIR/core/test002.c" \
    "$TEST_DIR/control/test019.c" \
    "$TEST_DIR/loops/test045.c" \
    "$TEST_DIR/globals/test060.c" \
    "$TEST_DIR/pointers/test034.c" \
    "$TEST_DIR/structs/test050.c" \
    "$TEST_DIR/strings/test070.c" \
    "$TEST_DIR/backend/x86_extern_ptr.c" \
    "$TEST_DIR/backend/x86_call_args.c" \
    "$TEST_DIR/backend/x86_narrow_load_store.c" \
    "$TEST_DIR/backend/x86_signed_unsigned_cmp.c" \
    "$TEST_DIR/backend/x86_pointer_arith.c" \
    "$TEST_DIR/backend/x86_global_initializers.c" \
    "$TEST_DIR/backend/x86_global_arrays.c" \
    "$TEST_DIR/backend/x86_static_locals.c" \
    "$TEST_DIR/backend/x86_global_struct_fields.c" \
    "$TEST_DIR/backend/x86_string_global_ptr.c" \
    "$TEST_DIR/backend/x86_recursive_calls.c" \
    "$TEST_DIR/backend/x86_nested_call_args.c" \
    "$TEST_DIR/backend/x86_mutual_calls.c" \
    "$TEST_DIR/backend/x86_short_circuit.c" \
    "$TEST_DIR/backend/x86_nested_break_continue.c" \
    "$TEST_DIR/backend/x86_funcptr_basic.c" \
    "$TEST_DIR/backend/x86_funcptr_reassign.c" \
    "$TEST_DIR/backend/x86_funcptr_global.c" \
    "$TEST_DIR/backend/x86_funcptr_struct.c" \
    "$TEST_DIR/backend/x86_indirect_call_args.c" \
    "$TEST_DIR/backend/x86_many_args.c" \
    "$TEST_DIR/backend/x86_stack_alignment_call.c" \
    "$TEST_DIR/backend/x86_local_struct_fields.c" \
    "$TEST_DIR/backend/x86_mixed_width_locals.c" \
    "$TEST_DIR/backend/x86_div_mod_signed.c" \
    "$TEST_DIR/backend/x86_div_mod_unsigned.c" \
    "$TEST_DIR/backend/x86_addr_of_local.c" \
    "$TEST_DIR/backend/x86_struct_ptr_fields.c" \
    "$TEST_DIR/backend/x86_local_array_index.c" \
    "$TEST_DIR/backend/x86_switch_basic.c" \
    "$TEST_DIR/backend/x86_conditional_expr.c" \
    "$TEST_DIR/backend/x86_pre_post_inc.c" \
    "$TEST_DIR/backend/x86_pointer_inc_store.c" \
    "$TEST_DIR/backend/x86_i64_add_sub.c" \
    "$TEST_DIR/backend/x86_i64_args_return_mix.c" \
    "$TEST_DIR/backend/x86_i64_compare.c" \
    "$TEST_DIR/backend/x86_i64_mul_div_mod.c" \
    "$TEST_DIR/backend/x86_varargs_sum.c" \
    "$TEST_DIR/backend/x86_varargs_mixed.c" \
    "$TEST_DIR/backend/x86_varargs_many.c" \
    "$TEST_DIR/backend/x86_struct_return_small.c" \
    "$TEST_DIR/backend/x86_struct_arg_by_value.c" \
    "$TEST_DIR/backend/x86_i64_shifts.c" \
    "$TEST_DIR/backend/x86_i64_bitwise.c" \
    "$TEST_DIR/backend/x86_casts_int_uint_i64_ptr.c" \
    "$TEST_DIR/backend/x86_nested_struct_fields.c" \
    "$TEST_DIR/backend/x86_array_of_structs.c" \
    "$TEST_DIR/backend/x86_struct_arg_mixed_scalars.c" \
    "$TEST_DIR/backend/x86_struct_arg_large.c" \
    "$TEST_DIR/backend/x86_nested_struct_return_call.c" \
    "$TEST_DIR/backend/x86_struct_return_with_args.c" \
    "$TEST_DIR/backend/x86_struct_return_medium.c"
do
    compile_one "$src"
done

if "$CC" -S -target=x86 -asm=gas -g "$TEST_DIR/debug/debug_lines.c" -o "$TMP/debug_lines.x86.gas.s" >/dev/null 2>"$TMP/debug_lines.x86.err" && [ -s "$TMP/debug_lines.x86.gas.s" ]; then
    echo "  PASS debug/debug_lines.c (-asm=gas -g)"
else
    echo "  FAIL debug/debug_lines.c (-asm=gas -g)"
    cat "$TMP/debug_lines.x86.err"
    fail=$((fail + 1))
fi

check "x86_extern_ptr.c extern address" \
    grep -q "mov eax, external_value" "$TMP/x86_extern_ptr.c.x86.s"

check "x86_call_args.c cdecl stack calls" sh -c \
    "grep -q 'call add5' '$TMP/x86_call_args.c.x86.s' &&
     grep -q 'call mix' '$TMP/x86_call_args.c.x86.s' &&
     grep -q 'add esp, 20' '$TMP/x86_call_args.c.x86.s' &&
     grep -q 'add esp, 12' '$TMP/x86_call_args.c.x86.s'"

check "x86_narrow_load_store.c narrow memory paths" sh -c \
    "grep -q 'mov \[ebp-8\], al' '$TMP/x86_narrow_load_store.c.x86.s' &&
     grep -q 'movzx eax, byte \[ebp-8\]' '$TMP/x86_narrow_load_store.c.x86.s' &&
     grep -q 'mov \[ebp-6\], ax' '$TMP/x86_narrow_load_store.c.x86.s' &&
     grep -q 'movzx eax, word \[ebp-6\]' '$TMP/x86_narrow_load_store.c.x86.s'"

check "x86_signed_unsigned_cmp.c signed/unsigned conditions" sh -c \
    "grep -q 'setl al' '$TMP/x86_signed_unsigned_cmp.c.x86.s' &&
     grep -q 'seta al' '$TMP/x86_signed_unsigned_cmp.c.x86.s'"

check "x86_pointer_arith.c scale-by-4 addressing" sh -c \
    "grep -q 'lea eax, \\[ebp-16\\]' '$TMP/x86_pointer_arith.c.x86.s' &&
     grep -q 'mov eax, \\[eax+4\\]' '$TMP/x86_pointer_arith.c.x86.s' &&
     grep -q 'mov eax, \\[ebp-8\\]' '$TMP/x86_pointer_arith.c.x86.s' &&
     grep -q 'mov eax, \\[ebp-20\\]' '$TMP/x86_pointer_arith.c.x86.s'"

check "x86_global_initializers.c scalar data directives" sh -c \
    "grep -q '\.long 40' '$TMP/x86_global_initializers.c.x86.s' &&
     grep -q '\.byte 7' '$TMP/x86_global_initializers.c.x86.s' &&
     grep -q '\.short 300' '$TMP/x86_global_initializers.c.x86.s'"

check "x86_global_arrays.c array data and 16-bit indexed load" sh -c \
    "grep -q '\.long 10' '$TMP/x86_global_arrays.c.x86.s' &&
     grep -q '\.byte 1, 2, 3, 4' '$TMP/x86_global_arrays.c.x86.s' &&
     grep -q '\.short 5' '$TMP/x86_global_arrays.c.x86.s' &&
     grep -q 'movzx eax, word \[gw+ebx\]' '$TMP/x86_global_arrays.c.x86.s'"

check "x86_static_locals.c static storage load/store" sh -c \
    "grep -q '__static_bump_counter' '$TMP/x86_static_locals.c.x86.s' &&
     grep -q 'mov DWORD PTR \[__static_bump_counter\], eax' '$TMP/x86_static_locals.c.x86.s'"

check "x86_global_struct_fields.c aggregate data and narrow fields" sh -c \
    "grep -q '\.byte 10, 0, 0, 0, 3, 0, 5, 0, 24, 0, 0, 0' '$TMP/x86_global_struct_fields.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \\[gp\\]' '$TMP/x86_global_struct_fields.c.x86.s' &&
     grep -q 'movzx eax, BYTE PTR \\[gp+4\\]' '$TMP/x86_global_struct_fields.c.x86.s' &&
     grep -q 'movzx eax, WORD PTR \\[gp+6\\]' '$TMP/x86_global_struct_fields.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \\[gp+8\\]' '$TMP/x86_global_struct_fields.c.x86.s'"

check "x86_string_global_ptr.c global pointer data/load" sh -c \
    "grep -Eq '^_?pmsg:' '$TMP/x86_string_global_ptr.c.x86.s' &&
     grep -Eq '^[[:space:]]*\.long[[:space:]]*_?msg' '$TMP/x86_string_global_ptr.c.x86.s' &&
     grep -Eq 'mov eax, DWORD PTR \[_?pmsg\]|mov eax, DWORD PTR \[pmsg\]' '$TMP/x86_string_global_ptr.c.x86.s'"

check "x86_recursive_calls.c recursive call/cleanup" sh -c \
    "grep -q 'call fact' '$TMP/x86_recursive_calls.c.x86.s' &&
     grep -q 'imul' '$TMP/x86_recursive_calls.c.x86.s' &&
     grep -q 'add esp, 4' '$TMP/x86_recursive_calls.c.x86.s'"

check "x86_nested_call_args.c nested calls and stack cleanup" sh -c \
    "grep -q 'call add' '$TMP/x86_nested_call_args.c.x86.s' &&
     grep -q 'call mul' '$TMP/x86_nested_call_args.c.x86.s' &&
     grep -q 'call mix' '$TMP/x86_nested_call_args.c.x86.s' &&
     grep -q 'add esp, 8' '$TMP/x86_nested_call_args.c.x86.s' &&
     grep -q 'add esp, 12' '$TMP/x86_nested_call_args.c.x86.s'"

check "x86_mutual_calls.c forward/mutual calls" sh -c \
    "grep -q 'call even' '$TMP/x86_mutual_calls.c.x86.s' &&
     grep -q 'call odd' '$TMP/x86_mutual_calls.c.x86.s' &&
     grep -q 'add esp, 4' '$TMP/x86_mutual_calls.c.x86.s'"

check "x86_short_circuit.c short-circuit branches" sh -c \
    "grep -q 'call bump' '$TMP/x86_short_circuit.c.x86.s' &&
     grep -q 'je L' '$TMP/x86_short_circuit.c.x86.s' &&
     grep -q 'jne L' '$TMP/x86_short_circuit.c.x86.s'"

check "x86_nested_break_continue.c nested loop control" sh -c \
    "grep -q 'jmp L' '$TMP/x86_nested_break_continue.c.x86.s' &&
     grep -q 'cmp ebx, eax' '$TMP/x86_nested_break_continue.c.x86.s' &&
     grep -Eq 'jge L|jne L' '$TMP/x86_nested_break_continue.c.x86.s'"

check "x86_funcptr_basic.c local indirect call" sh -c \
    "grep -q 'call ecx' '$TMP/x86_funcptr_basic.c.x86.s' &&
     grep -q 'add esp, 4' '$TMP/x86_funcptr_basic.c.x86.s'"

check "x86_funcptr_reassign.c reassigned indirect calls" sh -c \
    "[ \$(grep -c 'call ecx' '$TMP/x86_funcptr_reassign.c.x86.s') -ge 2 ] &&
     grep -q 'add esp, 4' '$TMP/x86_funcptr_reassign.c.x86.s'"

check "x86_funcptr_global.c global function pointer" sh -c \
    "grep -Eq '^_?gfp:' '$TMP/x86_funcptr_global.c.x86.s' &&
     grep -Eq '^[[:space:]]*\.long[[:space:]]*_?plus2' '$TMP/x86_funcptr_global.c.x86.s' &&
     grep -q 'call ecx' '$TMP/x86_funcptr_global.c.x86.s'"

check "x86_funcptr_struct.c struct field indirect call" sh -c \
    "grep -q 'call ecx' '$TMP/x86_funcptr_struct.c.x86.s' &&
     grep -q 'add esp, 8' '$TMP/x86_funcptr_struct.c.x86.s'"

check "x86_indirect_call_args.c multi-arg indirect call cleanup" sh -c \
    "grep -q 'call ecx' '$TMP/x86_indirect_call_args.c.x86.s' &&
     grep -q 'add esp, 20' '$TMP/x86_indirect_call_args.c.x86.s'"


check "x86_many_args.c many cdecl stack arguments" sh -c \
    "grep -q 'call x86_many_args_sum' '$TMP/x86_many_args.c.x86.s' &&
     grep -q '\[ebp' '$TMP/x86_many_args.c.x86.s' &&
     grep -q 'ret' '$TMP/x86_many_args.c.x86.s'"

check "x86_stack_alignment_call.c nested cdecl call cleanup" sh -c \
    "grep -q 'call callee' '$TMP/x86_stack_alignment_call.c.x86.s' &&
     grep -q 'add esp, 32' '$TMP/x86_stack_alignment_call.c.x86.s' &&
     grep -q '\[ebp' '$TMP/x86_stack_alignment_call.c.x86.s'"

check "x86_local_struct_fields.c local struct field stack offsets" sh -c \
    "grep -q 'sub esp,' '$TMP/x86_local_struct_fields.c.x86.s' &&
     grep -q '\[ebp-' '$TMP/x86_local_struct_fields.c.x86.s' &&
     grep -q '17' '$TMP/x86_local_struct_fields.c.x86.s' &&
     grep -q '25' '$TMP/x86_local_struct_fields.c.x86.s'"

check "x86_mixed_width_locals.c char/short/int stack locals" sh -c \
    "grep -q 'mov \[ebp-' '$TMP/x86_mixed_width_locals.c.x86.s' &&
     grep -q 'movzx eax, byte \[ebp-' '$TMP/x86_mixed_width_locals.c.x86.s' &&
     grep -q 'movzx eax, word \[ebp-' '$TMP/x86_mixed_width_locals.c.x86.s'"

check "x86_div_mod_signed.c signed divide/modulo" sh -c \
    "grep -q 'cdq' '$TMP/x86_div_mod_signed.c.x86.s' &&
     grep -q 'idiv' '$TMP/x86_div_mod_signed.c.x86.s' &&
     grep -q 'mov eax, edx' '$TMP/x86_div_mod_signed.c.x86.s'"

check "x86_div_mod_unsigned.c unsigned divide/modulo" sh -c \
    "grep -Eq 'xor edx, edx|mov edx, 0' '$TMP/x86_div_mod_unsigned.c.x86.s' &&
     grep -q 'div' '$TMP/x86_div_mod_unsigned.c.x86.s' &&
     grep -q 'mov eax, edx' '$TMP/x86_div_mod_unsigned.c.x86.s'"

check "x86_addr_of_local.c address-of local and pointer dereference" sh -c \
    "grep -q 'lea eax, \[ebp-' '$TMP/x86_addr_of_local.c.x86.s' &&
     grep -q 'mov eax, \[ebx+0\]' '$TMP/x86_addr_of_local.c.x86.s' &&
     grep -q 'mov \[ebx+0\], eax' '$TMP/x86_addr_of_local.c.x86.s'"

check "x86_struct_ptr_fields.c struct pointer field access" sh -c \
    "grep -q 'lea eax, \[ebp-8\]' '$TMP/x86_struct_ptr_fields.c.x86.s' &&
     grep -q 'mov eax, \[ebx+0\]' '$TMP/x86_struct_ptr_fields.c.x86.s' &&
     grep -q 'mov eax, \[ebx+4\]' '$TMP/x86_struct_ptr_fields.c.x86.s'"

check "x86_local_array_index.c local array indexed addressing" sh -c \
    "grep -q 'shl ebx, 2' '$TMP/x86_local_array_index.c.x86.s' &&
     grep -q '\[ebp-.*+ebx\]' '$TMP/x86_local_array_index.c.x86.s' &&
     grep -q 'mov eax, \[ebp-.*+ebx\]' '$TMP/x86_local_array_index.c.x86.s'"

check "x86_switch_basic.c switch compare/branch lowering" sh -c \
    "grep -q 'cmp' '$TMP/x86_switch_basic.c.x86.s' &&
     grep -Eq '\<je\>|\<jne\>' '$TMP/x86_switch_basic.c.x86.s' &&
     grep -q 'jmp' '$TMP/x86_switch_basic.c.x86.s' &&
     grep -q '10' '$TMP/x86_switch_basic.c.x86.s' &&
     grep -q '20' '$TMP/x86_switch_basic.c.x86.s' &&
     grep -q '50' '$TMP/x86_switch_basic.c.x86.s' &&
     grep -q '99' '$TMP/x86_switch_basic.c.x86.s'"

check "x86_conditional_expr.c ternary branch/merge lowering" sh -c \
    "grep -q 'cmp' '$TMP/x86_conditional_expr.c.x86.s' &&
     grep -Eq '\<je\>|\<jne\>' '$TMP/x86_conditional_expr.c.x86.s' &&
     grep -q '17' '$TMP/x86_conditional_expr.c.x86.s' &&
     grep -q '29' '$TMP/x86_conditional_expr.c.x86.s'"

check "x86_pre_post_inc.c pre/post increment locals" sh -c \
    "grep -q 'mov eax, 1' '$TMP/x86_pre_post_inc.c.x86.s' &&
     grep -q 'add eax, ebx' '$TMP/x86_pre_post_inc.c.x86.s' &&
     grep -q 'mov \[ebp-4\], eax' '$TMP/x86_pre_post_inc.c.x86.s' &&
     grep -q 'mov \[ebp-8\], eax' '$TMP/x86_pre_post_inc.c.x86.s' &&
     grep -q 'mov \[ebp-12\], eax' '$TMP/x86_pre_post_inc.c.x86.s'"

check "x86_pointer_inc_store.c pointer dereference increment store" sh -c \
    "grep -q 'lea eax, \[ebp-4\]' '$TMP/x86_pointer_inc_store.c.x86.s' &&
     grep -q 'mov eax, \[ecx\]' '$TMP/x86_pointer_inc_store.c.x86.s' &&
     grep -q 'mov eax, 1' '$TMP/x86_pointer_inc_store.c.x86.s' &&
     grep -q 'add eax, ebx' '$TMP/x86_pointer_inc_store.c.x86.s' &&
     grep -q 'mov \[ecx\], eax' '$TMP/x86_pointer_inc_store.c.x86.s'"

check "x86_i64_add_sub.c 64-bit add/sub lowering" sh -c \
    "grep -q 'add' '$TMP/x86_i64_add_sub.c.x86.s' &&
     grep -q 'sub' '$TMP/x86_i64_add_sub.c.x86.s' &&
     grep -q '\[ebp' '$TMP/x86_i64_add_sub.c.x86.s' &&
     grep -q 'ret' '$TMP/x86_i64_add_sub.c.x86.s'"

check "x86_i64_compare.c 64-bit compare lowering" sh -c \
    "grep -q 'cmp' '$TMP/x86_i64_compare.c.x86.s' &&
     grep -Eq '\<jl\>|\<jb\>|\<je\>|\<jne\>|\<jg\>|\<ja\>' '$TMP/x86_i64_compare.c.x86.s' &&
     grep -q '11' '$TMP/x86_i64_compare.c.x86.s' &&
     grep -q '22' '$TMP/x86_i64_compare.c.x86.s' &&
     grep -q '33' '$TMP/x86_i64_compare.c.x86.s'"

check "x86_i64_args_return_mix.c mixed i64/scalar cdecl ABI" sh -c \
    "grep -q 'x86_i64_args_return_mix' '$TMP/x86_i64_args_return_mix.c.x86.s' &&
     grep -q 'call' '$TMP/x86_i64_args_return_mix.c.x86.s'"

check "x86_i64_mul_div_mod.c 64-bit mul/div/mod lowering" sh -c \
    "grep -q 'imul eax, ebx' '$TMP/x86_i64_mul_div_mod.c.x86.s' &&
     grep -q 'cdq' '$TMP/x86_i64_mul_div_mod.c.x86.s' &&
     grep -q 'idiv ecx' '$TMP/x86_i64_mul_div_mod.c.x86.s' &&
     grep -q 'mov eax, edx' '$TMP/x86_i64_mul_div_mod.c.x86.s' &&
     grep -q 'call f' '$TMP/x86_i64_mul_div_mod.c.x86.s' &&
     grep -q 'add esp, 8' '$TMP/x86_i64_mul_div_mod.c.x86.s'"

check "x86_i64_bitwise.c 64-bit bitwise lowering" sh -c \
    "grep -q 'and' '$TMP/x86_i64_bitwise.c.x86.s' &&
     grep -q 'or' '$TMP/x86_i64_bitwise.c.x86.s' &&
     grep -q 'xor' '$TMP/x86_i64_bitwise.c.x86.s' &&
     grep -q 'not' '$TMP/x86_i64_bitwise.c.x86.s'"

check "x86_i64_shifts.c 64-bit shift lowering" sh -c \
    "grep -Eq 'shl|sal|sar|shr' '$TMP/x86_i64_shifts.c.x86.s' &&
     grep -Eq '\<cl\>|\<ecx\>' '$TMP/x86_i64_shifts.c.x86.s' &&
     grep -q 'ret' '$TMP/x86_i64_shifts.c.x86.s'"

check "x86_casts_int_uint_i64_ptr.c scalar/pointer cast lowering" sh -c \
    "grep -Eq 'cdq|movsx|movzx|mov' '$TMP/x86_casts_int_uint_i64_ptr.c.x86.s' &&
     grep -q 'lea eax, \[ebp-' '$TMP/x86_casts_int_uint_i64_ptr.c.x86.s' &&
     grep -q '1234' '$TMP/x86_casts_int_uint_i64_ptr.c.x86.s' &&
     grep -q '\[ebp-' '$TMP/x86_casts_int_uint_i64_ptr.c.x86.s'"

check "x86_nested_struct_fields.c nested aggregate data/accesses" sh -c \
    "grep -q 'global_outer:' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q '\.byte 10, 0, 0, 0, 3, 0, 20, 0, 44, 1, 0, 0, 160, 15, 0, 0' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \[global_outer\]' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q 'movzx eax, BYTE PTR \[global_outer+4\]' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q 'movzx eax, WORD PTR \[global_outer+6\]' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \[global_outer+8\]' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \[global_outer+12\]' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q '4333' '$TMP/x86_nested_struct_fields.c.x86.s' &&
     grep -q '5433' '$TMP/x86_nested_struct_fields.c.x86.s'"

check "x86_array_of_structs.c array-of-struct data/indexing" sh -c \
    "grep -q 'table:' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q '1, 0, 0, 0, 10, 0, 0, 0' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q '3, 0, 0, 0, 30, 0, 0, 0' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \[table\]' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \[table+12\]' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q 'mov eax, DWORD PTR \[table+16\]' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q 'mov eax, \[eax+4\]' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q '24' '$TMP/x86_array_of_structs.c.x86.s' &&
     grep -q '45' '$TMP/x86_array_of_structs.c.x86.s'"


check "x86_struct_arg_mixed_scalars.c mixed scalar/struct argument lowering" sh -c \
    "grep -q 'call use_mixed' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s' &&
     grep -q 'add esp, 20' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s' &&
     grep -q 'call use_mixed_twice' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s' &&
     grep -q 'sub esp, 8' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s' &&
     grep -q 'mov \[esp+4\], eax' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s' &&
     grep -q '54321' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s' &&
     grep -q '5433' '$TMP/x86_struct_arg_mixed_scalars.c.x86.s'"

check "x86_struct_arg_large.c byte-copy large struct argument" sh -c \
    "grep -q 'call use_big' '$TMP/x86_struct_arg_large.c.x86.s' &&
     grep -q 'sub esp, 20' '$TMP/x86_struct_arg_large.c.x86.s' &&
     grep -q 'mov \[esp+16\], eax' '$TMP/x86_struct_arg_large.c.x86.s' &&
     grep -q 'add esp, 24' '$TMP/x86_struct_arg_large.c.x86.s'"

check "x86_struct_return_medium.c medium struct return lowering" sh -c \
    "grep -q 'call make_medium' '$TMP/x86_struct_return_medium.c.x86.s' &&
     grep -q 'call consume_medium' '$TMP/x86_struct_return_medium.c.x86.s' &&
     grep -q 'add esp, 16' '$TMP/x86_struct_return_medium.c.x86.s' &&
     grep -q 'sub esp, 12' '$TMP/x86_struct_return_medium.c.x86.s' &&
     grep -q 'add esp, 12' '$TMP/x86_struct_return_medium.c.x86.s' &&
     grep -q '321' '$TMP/x86_struct_return_medium.c.x86.s' &&
     grep -q '654' '$TMP/x86_struct_return_medium.c.x86.s'"


check "x86_struct_return_with_args.c hidden return buffer plus args" sh -c \
    "grep -q 'call make_pair_with_args' '$TMP/x86_struct_return_with_args.c.x86.s' &&
     grep -q '\[ebp' '$TMP/x86_struct_return_with_args.c.x86.s' &&
     grep -q 'ret' '$TMP/x86_struct_return_with_args.c.x86.s'"


check "x86_nested_struct_return_call.c nested struct-return consumption" sh -c \
    "grep -q 'call make_pair_nested' '$TMP/x86_nested_struct_return_call.c.x86.s' &&
     grep -q 'call consume_pair_nested' '$TMP/x86_nested_struct_return_call.c.x86.s' &&
     grep -q '\[ebp' '$TMP/x86_nested_struct_return_call.c.x86.s' &&
     grep -q 'ret' '$TMP/x86_nested_struct_return_call.c.x86.s'"

check "x86_varargs_sum.c cdecl variadic stack access" sh -c \
    "grep -q 'call sum3' '$TMP/x86_varargs_sum.c.x86.s' &&
     grep -q 'add esp, 16' '$TMP/x86_varargs_sum.c.x86.s' &&
     grep -q '\[ebp+' '$TMP/x86_varargs_sum.c.x86.s'"

check "x86_varargs_mixed.c cdecl variadic argument layout" sh -c \
    "grep -q 'call pick4' '$TMP/x86_varargs_mixed.c.x86.s' &&
     grep -q 'add esp, 20' '$TMP/x86_varargs_mixed.c.x86.s' &&
     grep -q '\[ebp+' '$TMP/x86_varargs_mixed.c.x86.s'"

check "x86_varargs_many.c cdecl mixed variadic argument layout" sh -c \
    "grep -q 'call sum_mix' '$TMP/x86_varargs_many.c.x86.s' &&
     grep -q 'add esp,' '$TMP/x86_varargs_many.c.x86.s' &&
     grep -q '\[ebp+' '$TMP/x86_varargs_many.c.x86.s'"

check "x86_struct_return_small.c small struct return lowering" sh -c \
    "grep -q 'call make_pair' '$TMP/x86_struct_return_small.c.x86.s' &&
     grep -q '10' '$TMP/x86_struct_return_small.c.x86.s' &&
     grep -q '32' '$TMP/x86_struct_return_small.c.x86.s' &&
     grep -q '\[ebp-' '$TMP/x86_struct_return_small.c.x86.s'"

check "x86_struct_arg_by_value.c struct argument by value lowering" sh -c \
    "grep -q 'call sum_pair' '$TMP/x86_struct_arg_by_value.c.x86.s' &&
     grep -q '11' '$TMP/x86_struct_arg_by_value.c.x86.s' &&
     grep -q '31' '$TMP/x86_struct_arg_by_value.c.x86.s' &&
     grep -q '\[ebp' '$TMP/x86_struct_arg_by_value.c.x86.s'"

[ "$fail" -eq 0 ] || exit 1
echo "x86 smoke test OK"

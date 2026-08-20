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
    if "$CC" -S -target=x64 "$src" -o "$TMP/$base.x64.s" >/dev/null 2>"$TMP/$base.x64.err" && [ -s "$TMP/$base.x64.s" ]; then
        echo "  PASS $base"
    else
        echo "  FAIL $base"
        cat "$TMP/$base.x64.err"
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

echo "x64 assembly-generation smoke test:"

for src in \
    "$TEST_DIR/core/test001.c" \
    "$TEST_DIR/core/test002.c" \
    "$TEST_DIR/backend/x64_call_args.c" \
    "$TEST_DIR/backend/x64_global_initializers.c" \
    "$TEST_DIR/backend/x64_funcptr_basic.c" \
    "$TEST_DIR/backend/x64_struct_ptr_fields.c" \
    "$TEST_DIR/backend/x64_i64_add_sub.c" \
    "$TEST_DIR/backend/x64_div_mod_signed.c" \
    "$TEST_DIR/backend/x64_div_mod_unsigned.c" \
    "$TEST_DIR/backend/x64_conditional_expr.c" \
    "$TEST_DIR/backend/x64_switch_basic.c" \
    "$TEST_DIR/backend/x64_funcptr_global.c" \
    "$TEST_DIR/backend/x64_funcptr_struct.c" \
    "$TEST_DIR/backend/x64_static_locals.c" \
    "$TEST_DIR/backend/x64_global_arrays.c" \
    "$TEST_DIR/backend/x64_addr_of_local.c" \
    "$TEST_DIR/backend/x64_narrow_load_store.c" \
    "$TEST_DIR/backend/x64_signed_unsigned_cmp.c" \
    "$TEST_DIR/backend/x64_pointer_arith.c" \
    "$TEST_DIR/backend/x64_short_circuit.c" \
    "$TEST_DIR/backend/x64_nested_break_continue.c" \
    "$TEST_DIR/backend/x64_recursive_calls.c" \
    "$TEST_DIR/backend/x64_nested_call_args.c" \
    "$TEST_DIR/backend/x64_mutual_calls.c" \
    "$TEST_DIR/backend/x64_funcptr_reassign.c" \
    "$TEST_DIR/backend/x64_indirect_call_args.c" \
    "$TEST_DIR/backend/x64_local_struct_fields.c" \
    "$TEST_DIR/backend/x64_mixed_width_locals.c" \
    "$TEST_DIR/backend/x64_local_array_index.c" \
    "$TEST_DIR/backend/x64_pre_post_inc.c" \
    "$TEST_DIR/backend/x64_pointer_inc_store.c" \
    "$TEST_DIR/backend/x64_i64_compare.c" \
    "$TEST_DIR/backend/x64_i64_mul_div_mod.c" \
    "$TEST_DIR/backend/x64_varargs_sum.c" \
    "$TEST_DIR/backend/x64_varargs_mixed.c" \
    "$TEST_DIR/backend/x64_struct_return_small.c" \
    "$TEST_DIR/backend/x64_struct_arg_by_value.c" \
    "$TEST_DIR/backend/x64_i64_shifts.c" \
    "$TEST_DIR/backend/x64_i64_bitwise.c" \
    "$TEST_DIR/backend/x64_casts_int_uint_i64_ptr.c" \
    "$TEST_DIR/backend/x64_nested_struct_fields.c" \
    "$TEST_DIR/backend/x64_array_of_structs.c" \
    "$TEST_DIR/backend/x64_struct_arg_mixed_scalars.c" \
    "$TEST_DIR/backend/x64_struct_return_medium.c" \
    "$TEST_DIR/backend/x64_i64_args_return_mix.c" \
    "$TEST_DIR/backend/x64_many_args.c" \
    "$TEST_DIR/backend/x64_narrow_struct_fields.c" \
    "$TEST_DIR/backend/x64_nested_struct_return_call.c" \
    "$TEST_DIR/backend/x64_struct_return_with_args.c" \
    "$TEST_DIR/backend/x64_extern_ptr.c" \
    "$TEST_DIR/backend/x64_global_struct_fields.c" \
    "$TEST_DIR/backend/x64_string_global_ptr.c"
do
    compile_one "$src"
done

check "x64_call_args.c register/stack argument lowering" sh -c \
    "grep -Eq 'call[[:space:]]+_?mix' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-8], rdi' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-16], rsi' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-24], rdx' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-32], rcx' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-40], r8' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-48], r9' '$TMP/x64_call_args.c.x64.s' &&
     grep -Fq 'mov rax, QWORD PTR [rbp+16]' '$TMP/x64_call_args.c.x64.s'"

check "x64_global_initializers.c scalar data directives" sh -c \
    "grep -q '\.long 40' '$TMP/x64_global_initializers.c.x64.s' &&
     grep -q '\.byte 7' '$TMP/x64_global_initializers.c.x64.s' &&
     grep -q '\.short 300' '$TMP/x64_global_initializers.c.x64.s'"

check "x64_funcptr_basic.c indirect call lowering" sh -c \
    "grep -Eq 'call \\*%|call r|call qword|call \\*' '$TMP/x64_funcptr_basic.c.x64.s' ||
     grep -q 'call rax' '$TMP/x64_funcptr_basic.c.x64.s' ||
     grep -q 'call rcx' '$TMP/x64_funcptr_basic.c.x64.s'"

check "x64_struct_ptr_fields.c struct pointer field access" sh -c \
    "grep -Eq 'call[[:space:]]+_?sum' '$TMP/x64_struct_ptr_fields.c.x64.s' &&
     grep -Fq 'mov DWORD PTR [rbp-8], eax' '$TMP/x64_struct_ptr_fields.c.x64.s' &&
     grep -Fq 'mov DWORD PTR [rbp-4], eax' '$TMP/x64_struct_ptr_fields.c.x64.s' &&
     grep -Fq 'lea rax, [rbp-8]' '$TMP/x64_struct_ptr_fields.c.x64.s' &&
     grep -Eq 'movsxd rax, DWORD PTR \\[r10\\+0\\]|mov eax, DWORD PTR \\[r10\\+0\\]' '$TMP/x64_struct_ptr_fields.c.x64.s' &&
     grep -Eq 'movsxd rax, DWORD PTR \\[r10\\+4\\]|mov eax, DWORD PTR \\[r10\\+4\\]' '$TMP/x64_struct_ptr_fields.c.x64.s'"

check "x64_i64_add_sub.c 64-bit add/sub lowering" sh -c \
    "grep -q 'add' '$TMP/x64_i64_add_sub.c.x64.s' &&
     grep -q 'sub' '$TMP/x64_i64_add_sub.c.x64.s' &&
     grep -Eq '\<rax\>|\<eax\>' '$TMP/x64_i64_add_sub.c.x64.s' &&
     grep -q 'ret' '$TMP/x64_i64_add_sub.c.x64.s'"

check "x64_div_mod_signed.c signed divide/modulo" sh -c \
    "grep -q 'idiv' '$TMP/x64_div_mod_signed.c.x64.s' &&
     grep -Eq 'cdq|cqo' '$TMP/x64_div_mod_signed.c.x64.s' &&
     grep -q 'imul' '$TMP/x64_div_mod_signed.c.x64.s'"

check "x64_div_mod_unsigned.c unsigned divide/modulo" sh -c \
    "grep -q 'div' '$TMP/x64_div_mod_unsigned.c.x64.s' &&
     grep -Eq 'xor edx, edx|xor rdx, rdx' '$TMP/x64_div_mod_unsigned.c.x64.s'"

check "x64_conditional_expr.c ternary branch/merge lowering" sh -c \
    "grep -Eq 'je|jne|jmp' '$TMP/x64_conditional_expr.c.x64.s' &&
     grep -Eq 'L[0-9]+' '$TMP/x64_conditional_expr.c.x64.s'"

check "x64_switch_basic.c switch compare/branch lowering" sh -c \
    "grep -q 'cmp' '$TMP/x64_switch_basic.c.x64.s' &&
     grep -Eq 'je|jne|jmp' '$TMP/x64_switch_basic.c.x64.s' &&
     grep -Fq 'mov rax, 50' '$TMP/x64_switch_basic.c.x64.s'"

check "x64_funcptr_global.c global function pointer" sh -c \
    "grep -q '_gfp' '$TMP/x64_funcptr_global.c.x64.s' &&
     grep -Eq '\.quad[[:space:]]+_?add3|\.long[[:space:]]+_?add3' '$TMP/x64_funcptr_global.c.x64.s' &&
     grep -Eq 'call[[:space:]]+\*|call[[:space:]]+r|call[[:space:]]+qword|call[[:space:]]+QWORD' '$TMP/x64_funcptr_global.c.x64.s'"

check "x64_funcptr_struct.c struct field indirect call" sh -c \
    "grep -Eq 'call[[:space:]]+\*|call[[:space:]]+r|call[[:space:]]+qword|call[[:space:]]+QWORD' '$TMP/x64_funcptr_struct.c.x64.s' &&
     grep -Eq '19|23' '$TMP/x64_funcptr_struct.c.x64.s'"

check "x64_static_locals.c static storage load/store" sh -c \
    "grep -q '_f.x' '$TMP/x64_static_locals.c.x64.s' &&
     grep -q '.long 40' '$TMP/x64_static_locals.c.x64.s' &&
     grep -q 'add' '$TMP/x64_static_locals.c.x64.s'"

check "x64_global_arrays.c array data and indexed load" sh -c \
    "grep -q '_gi' '$TMP/x64_global_arrays.c.x64.s' &&
     grep -q '_gs' '$TMP/x64_global_arrays.c.x64.s' &&
     grep -q '.long 10' '$TMP/x64_global_arrays.c.x64.s' &&
     grep -q '.short 1' '$TMP/x64_global_arrays.c.x64.s' &&
     grep -Eq 'DWORD PTR|WORD PTR' '$TMP/x64_global_arrays.c.x64.s'"

check "x64_addr_of_local.c address-of local and pointer dereference" sh -c \
    "grep -Fq 'lea rax, [rbp-' '$TMP/x64_addr_of_local.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-' '$TMP/x64_addr_of_local.c.x64.s' &&
     grep -Eq 'movsxd rax, DWORD PTR \\[r10\\+0\\]|mov eax, DWORD PTR \\[rax\\+0\\]|mov eax, DWORD PTR \\[r10\\+0\\]' '$TMP/x64_addr_of_local.c.x64.s'"

check "x64_narrow_load_store.c narrow memory paths" sh -c \
    "grep -Eq 'BYTE PTR|byte ptr' '$TMP/x64_narrow_load_store.c.x64.s' &&
     grep -Eq 'WORD PTR|word ptr' '$TMP/x64_narrow_load_store.c.x64.s' &&
     grep -q '_gc' '$TMP/x64_narrow_load_store.c.x64.s' &&
     grep -q '_gs' '$TMP/x64_narrow_load_store.c.x64.s'"

check "x64_signed_unsigned_cmp.c signed/unsigned conditions" sh -c \
    "grep -q 'cmp' '$TMP/x64_signed_unsigned_cmp.c.x64.s' &&
     grep -Eq 'jl|jge|jg|jle|setl|setge|setg|setle' '$TMP/x64_signed_unsigned_cmp.c.x64.s' &&
     grep -Eq 'jb|jae|ja|jbe|setb|setae|seta|setbe' '$TMP/x64_signed_unsigned_cmp.c.x64.s'"

check "x64_pointer_arith.c scale-by-4 addressing" sh -c \
    "grep -Eq 'imul|sal|shl|\\*4|,4|add|\\+8' '$TMP/x64_pointer_arith.c.x64.s' &&
     grep -Fq 'lea rax, [rbp-' '$TMP/x64_pointer_arith.c.x64.s' &&
     grep -Eq 'movsxd rax, DWORD PTR \\[r10\\+8\\]|mov eax, DWORD PTR \\[rax\\+8\\]|mov eax, DWORD PTR \\[r10\\+8\\]' '$TMP/x64_pointer_arith.c.x64.s'"

check "x64_short_circuit.c short-circuit branches" sh -c \
    "grep -q 'cmp' '$TMP/x64_short_circuit.c.x64.s' &&
     grep -Eq 'je|jne|jmp' '$TMP/x64_short_circuit.c.x64.s' &&
     grep -q '11' '$TMP/x64_short_circuit.c.x64.s' &&
     grep -q '22' '$TMP/x64_short_circuit.c.x64.s' &&
     grep -q '33' '$TMP/x64_short_circuit.c.x64.s'"

check "x64_nested_break_continue.c nested loop control" sh -c \
    "grep -q 'cmp' '$TMP/x64_nested_break_continue.c.x64.s' &&
     grep -Eq 'je|jne|jmp|jl|jge' '$TMP/x64_nested_break_continue.c.x64.s' &&
     grep -q '4' '$TMP/x64_nested_break_continue.c.x64.s' &&
     grep -q '3' '$TMP/x64_nested_break_continue.c.x64.s' &&
     grep -q '2' '$TMP/x64_nested_break_continue.c.x64.s'"

check "x64_recursive_calls.c recursive call" sh -c \
    "grep -Eq 'call[[:space:]]+_?fact' '$TMP/x64_recursive_calls.c.x64.s' &&
     grep -q 'imul' '$TMP/x64_recursive_calls.c.x64.s' &&
     grep -q 'cmp' '$TMP/x64_recursive_calls.c.x64.s'"

check "x64_nested_call_args.c nested calls and argument setup" sh -c \
    "grep -Eq 'call[[:space:]]+_?mul' '$TMP/x64_nested_call_args.c.x64.s' &&
     grep -Eq 'call[[:space:]]+_?add' '$TMP/x64_nested_call_args.c.x64.s' &&
     grep -q 'imul' '$TMP/x64_nested_call_args.c.x64.s'"

check "x64_mutual_calls.c forward/mutual calls" sh -c \
    "grep -Eq 'call[[:space:]]+_?odd' '$TMP/x64_mutual_calls.c.x64.s' &&
     grep -Eq 'call[[:space:]]+_?even' '$TMP/x64_mutual_calls.c.x64.s' &&
     grep -q 'cmp' '$TMP/x64_mutual_calls.c.x64.s'"

check "x64_funcptr_reassign.c reassigned indirect calls" sh -c \
    "grep -Eq '_?add1' '$TMP/x64_funcptr_reassign.c.x64.s' &&
     grep -Eq '_?add2' '$TMP/x64_funcptr_reassign.c.x64.s' &&
     grep -Eq 'call[[:space:]]+\\*|call[[:space:]]+r|call[[:space:]]+qword|call[[:space:]]+QWORD' '$TMP/x64_funcptr_reassign.c.x64.s'"

check "x64_indirect_call_args.c multi-arg indirect call" sh -c \
    "grep -Eq 'call[[:space:]]+\\*|call[[:space:]]+r|call[[:space:]]+qword|call[[:space:]]+QWORD' '$TMP/x64_indirect_call_args.c.x64.s' &&
     grep -q 'mov rdi' '$TMP/x64_indirect_call_args.c.x64.s' &&
     grep -q 'mov rsi' '$TMP/x64_indirect_call_args.c.x64.s' &&
     grep -q 'mov rdx' '$TMP/x64_indirect_call_args.c.x64.s' &&
     grep -q 'mov rcx' '$TMP/x64_indirect_call_args.c.x64.s'"

check "x64_local_struct_fields.c local struct field stack offsets" sh -c \
    "grep -Eq 'BYTE PTR|byte ptr' '$TMP/x64_local_struct_fields.c.x64.s' &&
     grep -Eq 'WORD PTR|word ptr' '$TMP/x64_local_struct_fields.c.x64.s' &&
     grep -Eq 'DWORD PTR|dword ptr' '$TMP/x64_local_struct_fields.c.x64.s' &&
     grep -q '300' '$TMP/x64_local_struct_fields.c.x64.s' &&
     grep -q '4000' '$TMP/x64_local_struct_fields.c.x64.s'"

check "x64_mixed_width_locals.c char/short/int stack locals" sh -c \
    "grep -Eq 'BYTE PTR|byte ptr' '$TMP/x64_mixed_width_locals.c.x64.s' &&
     grep -Eq 'WORD PTR|word ptr' '$TMP/x64_mixed_width_locals.c.x64.s' &&
     grep -Eq 'DWORD PTR|dword ptr' '$TMP/x64_mixed_width_locals.c.x64.s' &&
     grep -q '300' '$TMP/x64_mixed_width_locals.c.x64.s' &&
     grep -q '4000' '$TMP/x64_mixed_width_locals.c.x64.s'"

check "x64_local_array_index.c local array indexed addressing" sh -c \
    "grep -Eq 'movsxd[[:space:]]+r10' '$TMP/x64_local_array_index.c.x64.s' &&
     grep -Fq 'shl r10, 2' '$TMP/x64_local_array_index.c.x64.s' &&
     grep -Fq 'mov eax, DWORD PTR [rbp-16+r10]' '$TMP/x64_local_array_index.c.x64.s'"

check "x64_pre_post_inc.c pre/post increment locals" sh -c \
    "grep -q 'add' '$TMP/x64_pre_post_inc.c.x64.s' &&
     grep -Eq 'mov DWORD PTR \\[rbp-|DWORD PTR' '$TMP/x64_pre_post_inc.c.x64.s' &&
     grep -q '10' '$TMP/x64_pre_post_inc.c.x64.s'"

check "x64_pointer_inc_store.c pointer dereference increment store" sh -c \
    "grep -Fq 'lea rax, [rbp-4]' '$TMP/x64_pointer_inc_store.c.x64.s' &&
     grep -Fq 'mov QWORD PTR [rbp-12], rax' '$TMP/x64_pointer_inc_store.c.x64.s' &&
     grep -Eq 'mov eax, (DWORD PTR \\[rax\\]|\\[r11\\])' '$TMP/x64_pointer_inc_store.c.x64.s' &&
     grep -Fq 'mov [r11], eax' '$TMP/x64_pointer_inc_store.c.x64.s' &&
     grep -q 'add' '$TMP/x64_pointer_inc_store.c.x64.s'"

check "x64_i64_compare.c 64-bit compare lowering" sh -c \
    "grep -q 'cmp' '$TMP/x64_i64_compare.c.x64.s' &&
     grep -Eq 'setl|setg|sete|jl|jg|je' '$TMP/x64_i64_compare.c.x64.s' &&
     grep -Eq 'rax|r10|r11' '$TMP/x64_i64_compare.c.x64.s'"

check "x64_i64_mul_div_mod.c 64-bit mul/div/mod lowering" sh -c \
    "grep -q 'imul' '$TMP/x64_i64_mul_div_mod.c.x64.s' &&
     grep -q 'idiv' '$TMP/x64_i64_mul_div_mod.c.x64.s' &&
     grep -Eq 'cqo|cdq' '$TMP/x64_i64_mul_div_mod.c.x64.s' &&
     grep -Eq 'rdx|edx' '$TMP/x64_i64_mul_div_mod.c.x64.s'"

check "x64_i64_shifts.c 64-bit shift lowering" sh -c \
    "grep -Eq 'shl|sal|sar|shr' '$TMP/x64_i64_shifts.c.x64.s' &&
     grep -Eq '\<cl\>|\<ecx\>|\<rcx\>' '$TMP/x64_i64_shifts.c.x64.s' &&
     grep -q 'ret' '$TMP/x64_i64_shifts.c.x64.s'"

check "x64_i64_bitwise.c 64-bit bitwise lowering" sh -c \
    "grep -q 'and' '$TMP/x64_i64_bitwise.c.x64.s' &&
     grep -q 'or' '$TMP/x64_i64_bitwise.c.x64.s' &&
     grep -q 'xor' '$TMP/x64_i64_bitwise.c.x64.s' &&
     grep -q 'not' '$TMP/x64_i64_bitwise.c.x64.s'"

check "x64_casts_int_uint_i64_ptr.c scalar/pointer cast lowering" sh -c \
    "grep -Eq 'movsxd|cdqe|movsx|movzx|mov' '$TMP/x64_casts_int_uint_i64_ptr.c.x64.s' &&
     grep -Eq 'lea rax, \[rbp-|lea.*rbp' '$TMP/x64_casts_int_uint_i64_ptr.c.x64.s' &&
     grep -Eq 'QWORD PTR|DWORD PTR' '$TMP/x64_casts_int_uint_i64_ptr.c.x64.s' &&
     grep -q '1234' '$TMP/x64_casts_int_uint_i64_ptr.c.x64.s'"

check "x64_nested_struct_fields.c nested aggregate data/accesses" sh -c \
    "grep -q '_global_outer' '$TMP/x64_nested_struct_fields.c.x64.s' &&
     grep -q '\.byte 10, 0, 0, 0, 3, 0, 20, 0, 44, 1, 0, 0, 160, 15, 0, 0' '$TMP/x64_nested_struct_fields.c.x64.s' &&
     grep -q 'BYTE PTR' '$TMP/x64_nested_struct_fields.c.x64.s' &&
     grep -q 'WORD PTR' '$TMP/x64_nested_struct_fields.c.x64.s' &&
     grep -q '4333' '$TMP/x64_nested_struct_fields.c.x64.s' &&
     grep -q '5433' '$TMP/x64_nested_struct_fields.c.x64.s'"

check "x64_array_of_structs.c array-of-struct data/indexing" sh -c \
    "grep -q '_table' '$TMP/x64_array_of_structs.c.x64.s' &&
     grep -q '\.byte 1, 0, 0, 0, 10, 0, 0, 0' '$TMP/x64_array_of_structs.c.x64.s' &&
     grep -q '\.byte 3, 0, 0, 0, 30, 0, 0, 0' '$TMP/x64_array_of_structs.c.x64.s' &&
     grep -Eq 'DWORD PTR \\[r10 \\+ 12\\]|DWORD PTR \\[r10\\+12\\]|DWORD PTR \\[rax\\+4\\]' '$TMP/x64_array_of_structs.c.x64.s' &&
     grep -q '24' '$TMP/x64_array_of_structs.c.x64.s' &&
     grep -q '45' '$TMP/x64_array_of_structs.c.x64.s'"


check "x64_struct_arg_mixed_scalars.c mixed scalar/struct argument lowering" sh -c \
    "grep -q 'call _use_mixed' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s' &&
     grep -q 'call _use_mixed_twice' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s' &&
     grep -q 'mov rdi, QWORD PTR \[rsp\]' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s' &&
     grep -q 'mov rsi, QWORD PTR \[rsp\]' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s' &&
     grep -q 'mov rdx, QWORD PTR \[rsp\]' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s' &&
     grep -q '54321' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s' &&
     grep -q '5433' '$TMP/x64_struct_arg_mixed_scalars.c.x64.s'"

check "x64_struct_return_medium.c medium struct return lowering" sh -c \
    "grep -q 'call _make_medium' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -q 'call _consume_medium' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -q 'lea rax, \[rbp-12\]' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -q 'lea rax, \[rbp-24\]' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -Eq 'DWORD PTR \\[r11\\+0\\]|DWORD PTR \\[r10\\+0\\]' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -Eq 'DWORD PTR \\[r11\\+8\\]|DWORD PTR \\[r10\\+8\\]' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -q '321' '$TMP/x64_struct_return_medium.c.x64.s' &&
     grep -q '654' '$TMP/x64_struct_return_medium.c.x64.s'"

check "x64_varargs_sum.c x64 stdarg helper uses current Intel asm dialect" sh -c \
    "grep -Fq 'mov rax, QWORD PTR [rbp]' '$TMP/x64_varargs_sum.c.x64.s' &&
     grep -Fq 'add rax, 16' '$TMP/x64_varargs_sum.c.x64.s'"

check "x64_varargs_sum.c variadic argument access" sh -c \
    "grep -Eq 'va_|__builtin|__va|ap|rbp' '$TMP/x64_varargs_sum.c.x64.s' &&
     grep -q 'call _sum' '$TMP/x64_varargs_sum.c.x64.s' &&
     grep -Eq 'mov rdi|mov edi' '$TMP/x64_varargs_sum.c.x64.s'"

check "x64_varargs_mixed.c mixed variadic argument layout" sh -c \
    "grep -q 'call _mix' '$TMP/x64_varargs_mixed.c.x64.s' &&
     grep -Eq 'mov QWORD PTR \\[rbp-72\\], rsi|mov QWORD PTR \\[rbp-64\\], rdx|mov QWORD PTR \\[rbp-56\\], rcx' '$TMP/x64_varargs_mixed.c.x64.s' &&
     grep -Eq 'mov DWORD PTR \\[rbp-20\\], eax|mov QWORD PTR \\[rbp-28\\], rax|mov DWORD PTR \\[rbp-32\\], eax' '$TMP/x64_varargs_mixed.c.x64.s'"

check "x64_struct_return_small.c small struct return lowering" sh -c \
    "grep -Eq 'call[[:space:]]+_?make_pair' '$TMP/x64_struct_return_small.c.x64.s' &&
     grep -Eq 'mov.*rax|rax' '$TMP/x64_struct_return_small.c.x64.s' &&
     grep -Eq '19|23' '$TMP/x64_struct_return_small.c.x64.s'"

check "x64_struct_arg_by_value.c struct argument by value lowering" sh -c \
    "grep -Eq 'call[[:space:]]+_?sum_pair' '$TMP/x64_struct_arg_by_value.c.x64.s' &&
     grep -Eq 'mov rdi|mov edi|push|sub rsp' '$TMP/x64_struct_arg_by_value.c.x64.s' &&
     grep -Eq '19|23' '$TMP/x64_struct_arg_by_value.c.x64.s'"

check "x64_extern_ptr.c extern address" sh -c \
    "grep -q '_ext_value' '$TMP/x64_extern_ptr.c.x64.s' &&
     grep -Eq 'lea|OFFSET|rip|quad' '$TMP/x64_extern_ptr.c.x64.s'"

check "x64_global_struct_fields.c aggregate data and narrow fields" sh -c \
    "grep -q '_g' '$TMP/x64_global_struct_fields.c.x64.s' &&
     grep -Fq '.byte 7, 0, 44, 1, 160, 15, 0, 0' '$TMP/x64_global_struct_fields.c.x64.s' &&
     grep -Fq 'lea r10, [rip + ' '$TMP/x64_global_struct_fields.c.x64.s' &&
     grep -Fq 'movzx eax, BYTE PTR [r10]' '$TMP/x64_global_struct_fields.c.x64.s' &&
     grep -Fq 'movzx eax, WORD PTR [r10 + 2]' '$TMP/x64_global_struct_fields.c.x64.s' &&
     grep -Fq 'movsxd rax, DWORD PTR [r10 + 4]' '$TMP/x64_global_struct_fields.c.x64.s'"

check "x64_string_global_ptr.c global pointer data/load" sh -c \
    "grep -q '_msg' '$TMP/x64_string_global_ptr.c.x64.s' &&
     grep -q '_pmsg' '$TMP/x64_string_global_ptr.c.x64.s' &&
     grep -Eq '\\.quad[[:space:]]+_?msg|\\.long[[:space:]]+_?msg' '$TMP/x64_string_global_ptr.c.x64.s' &&
     grep -Eq 'BYTE PTR|byte ptr' '$TMP/x64_string_global_ptr.c.x64.s'"

check "x64_many_args.c register/stack overflow argument lowering" sh -c \
    "grep -q 'call _x64_many_args_sum' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'mov rdi, QWORD PTR' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'mov rsi, QWORD PTR' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'mov rdx, QWORD PTR' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'mov rcx, QWORD PTR' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'mov r8, QWORD PTR' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'mov r9, QWORD PTR' '$TMP/x64_many_args.c.x64.s' &&
     grep -q 'add rsp, 32' '$TMP/x64_many_args.c.x64.s'"

check "x64_i64_args_return_mix.c mixed i64/scalar ABI lowering" sh -c \
    "grep -q 'call _x64_i64_args_return_mix' '$TMP/x64_i64_args_return_mix.c.x64.s' &&
     grep -q 'call _x64_i64_args_return_mix_chain' '$TMP/x64_i64_args_return_mix.c.x64.s' &&
     grep -Eq 'mov rax|add rax|sub rax|sar rax|shl rax' '$TMP/x64_i64_args_return_mix.c.x64.s'"

check "x64_narrow_struct_fields.c narrow local struct fields" sh -c \
    "grep -q 'movzx eax, BYTE PTR' '$TMP/x64_narrow_struct_fields.c.x64.s' &&
     grep -q 'movzx eax, WORD PTR' '$TMP/x64_narrow_struct_fields.c.x64.s'"

check "x64_nested_struct_return_call.c nested struct-return consumption" sh -c \
    "grep -q 'call _make_pair_nested' '$TMP/x64_nested_struct_return_call.c.x64.s' &&
     grep -q 'call _consume_pair_nested' '$TMP/x64_nested_struct_return_call.c.x64.s'"

check "x64_struct_return_with_args.c struct return with live arguments" sh -c \
    "grep -q 'call _make_pair_with_args' '$TMP/x64_struct_return_with_args.c.x64.s' &&
     grep -q 'call _use_pair_with_args' '$TMP/x64_struct_return_with_args.c.x64.s' &&
     grep -q 'lea rax, \[rbp-16\]' '$TMP/x64_struct_return_with_args.c.x64.s'"

[ "$fail" -eq 0 ] || exit 1

echo "x64 smoke test OK"

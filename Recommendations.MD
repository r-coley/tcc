# TCC Project Recommendations

See also:

- `docs/BUILD_AND_PERF.md` for current stage naming, Make targets, timing flags, sqlite benchmark commands, and the next optimization plan
- `docs/ISO_C_PRODUCTION_PLAN.md` for the C99/C11 production conformance roadmap

## Current validated status

The compiler is currently in a strong green state after the latest preprocessor fixes, spaced-hash directive hardening, x64/x86 backend smoke expansion, x64 runtime execution coverage, the recent IR/type-system fixes around runtime VLAs, struct assignment/copy, bitfields, indexed inc/dec lowering, first-class `void` / `_Bool` / `enum` / `union` / function-pointee / float-double scaffolding, structural qualifier tracking on `Type`, modifiable-lvalue diagnostics, old-style/prototype function compatibility across preserved function pointee types and repeated declarations, qualifier-aware function-type compatibility for parameter pointees versus top-level parameter qualifiers, broader direct-spelling function-pointer declarator support for block-scope, static locals, nested static local spellings, nested global/extern object spellings, restored block-scope typedef/tag/enum namespace handling, stricter global object redeclaration checking for function-pointer objects and incomplete arrays, parenthesized pointer-to-incomplete-array declarator support in extern/global object declarations, and symmetric pointer/integer implicit-conversion diagnostics across the main local frontend paths.

Most recent focused semantic confirmation beyond that full-suite snapshot:

```text
nested pointer compatibility diagnostics:
  tests/:runtests.sh --filter incompatible_pointer_to_pointer
  5/5 completed, 0 failed

global function-pointer initializer compatibility:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-initcompat
  1140/1140 completed, 0 failed

local aggregate function-pointer initializer compatibility:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-local-fnptr
  1142/1142 completed, 0 failed

local function-pointer array initializer compatibility:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-local-fparray
  1144/1144 completed, 0 failed

local function-pointer array designator compatibility:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-local-fparray-design
  1146/1146 completed, 0 failed

direct-spelling local/static function-pointer array declarators:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-direct-fnptr-2
  1150/1150 completed, 0 failed

direct-spelling static local function-pointer declarators:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-static-fp-direct
  1152/1152 completed, 0 failed

nested direct-spelling static local function-pointer declarators:
  tests/:runtests.sh -c build/tcc -t arm64 -a clang -m tests/manifest.txt -D build/tmp-static-nested-fp-direct --filter static_local_nested_function_pointer_direct
  2/2 completed, 0 failed
```

Latest confirmed full regression:

```text
make stage2:
  bootstrap/stress checks: PASS

make test:
  build/tcc:        1184/1184 completed, 0 failed
  build/tcc_stage1: 1184/1184 completed, 0 failed
```

Current practical compiler classification:

```text
A strong small freestanding C89/C99-style integer-and-basic-floating compiler subset
with selected C11/C23 syntax gates, useful preprocessing, multi-target
assembly generation, and growing runtime smoke coverage.

It is not yet a complete ISO C hosted compiler.
```

## Recently completed work

### Runtime VLA, struct assignment, and indexed inc/dec IR fixes

Recent frontend/IR work closed several practical correctness gaps:

```text
runtime VLA struct element assignment
broader local/global struct assignment and copy lowering
bitfield member-pointer inc/dec result semantics
indexed/global-indexed lvalue inc/dec lowering in IR
ordinary post-inc/post-dec expression-value preservation in IR
```

New regression coverage includes:

```text
tests/type_semantics/test_runtime_vla_struct_element_assign.c
tests/type_semantics/test_runtime_vla_semantics.c
tests/type_semantics/test_struct_global_local_assign.c
tests/type_semantics/test_ir_incdec_indexed_lvalue.c
```

Most recent confirmation:

```text
make test:
  build/tcc:        1184/1184 completed, 0 failed
```

### Typedef redeclaration checking

Duplicate typedef declarations now reject incompatible type redefinitions
instead of being silently accepted.

Covered behavior:

```text
same-type typedef repeats remain accepted
incompatible typedef repeats now fail with a conflicting-typedef diagnostic
inner block typedef shadowing now works
block-local typedef names no longer leak after scope exit
block-scope struct tags now shadow and restore correctly
block-scope enum constants no longer leak after scope exit
```

Regression coverage:

```text
tests/errors/conflicting_typedef_redecl_char.c
tests/type_semantics/test_enum_const_block_no_leak.c
tests/type_semantics/test_struct_tag_block_shadowing.c
tests/type_semantics/test_typedef_block_shadowing.c
tests/type_semantics/test_typedef_block_no_leak.c
tests/type_semantics/test_typedef_redecl_same_type.c
```

### Qualifier-aware function-type compatibility

Function compatibility now distinguishes nested/pointee qualifiers that affect
the actual parameter type while still ignoring top-level qualifiers on function
parameters, matching standard C function-type rules more closely.

Covered behavior:

```text
prototype redeclarations now reject parameter pointee qualifier mismatches such as const int * versus int *
prototype redeclarations now reject nested pointer qualifier mismatches such as int *restrict * versus int **
top-level parameter qualifiers such as int *const remain compatible in repeated function declarations
function pointers that differ only by top-level parameter qualifiers now remain assignment-compatible
```

Regression coverage:

```text
tests/errors/prototype_redecl_incompatible_param_nested_restrict.c
tests/errors/prototype_redecl_incompatible_param_pointee_const.c
tests/type_semantics/test_function_pointer_param_top_level_const_compatible.c
tests/type_semantics/test_function_redecl_param_top_level_const_compatible.c
```

### Old-style function definition parameter declarations

K&R-style function definitions now accept ordinary pointer parameter
declarations after the identifier list, including grouped declarators such as
`int *a, *b;` and mixed adjusted forms such as `int a[], *b;`, and those
declarations participate in prototype compatibility checking using old-style
promotion rules.

Covered behavior:

```text
old-style function definitions such as int f(p) int *p; { ... } now parse and compile
grouped K&R declarators such as int *a, *b; now preserve the right type per parameter instead of over-applying pointer levels to later declarators
mixed grouped K&R declarators such as int a[], *b; now parse and retain the normal array-to-pointer adjustment behavior
prior prototypes remain compatible when the old-style parameter declaration differs only by ignored top-level qualifiers
prior prototypes still reject incompatible pointee qualifiers in old-style parameter declarations
```

Regression coverage:

```text
tests/errors/oldstyle_pointer_param_proto_incompatible_pointee_const.c
tests/type_semantics/test_oldstyle_grouped_array_pointer_params.c
tests/type_semantics/test_oldstyle_grouped_array_pointer_params_proto_compatible.c
tests/type_semantics/test_oldstyle_grouped_pointer_params.c
tests/type_semantics/test_oldstyle_pointer_param_definition.c
tests/type_semantics/test_oldstyle_pointer_param_proto_compatible.c
```

### First-class function pointee typing

Function pointers and abstract function-pointer declarators now preserve a real
function pointee type instead of lowering immediately to `pointer to int`.

Covered behavior:

```text
function-pointer typedefs preserve a TY_FUNC pointee
function-pointer parameters preserve a TY_FUNC pointee
abstract casts like ((int (*)(int)) fp) preserve a TY_FUNC pointee
function addresses are modeled as pointer-to-function rather than pointer-to-int
```

Regression coverage:

```text
tests/type_semantics/test_function_pointer_typedef_param.c
tests/types/test234.c
tests/torture/00209.c
```

### First-class `float` / `double` scaffolding

`float` and `double` now exist as real type kinds with correct size/alignment
tracking instead of being lowered to fake `int`/pointer stand-ins.

Covered behavior:

```text
float is preserved as a 4-byte opaque scalar type
double is preserved as an 8-byte opaque scalar type
block-scope float/double locals keep truthful non-pointer types
struct/union field layout uses real float/double sizes and alignment
```

Regression coverage:

```text
tests/type_semantics/test_float_double_local_type_layout.c
tests/types/test_alignof.c
```

### Structural qualifier tracking

The type system now preserves qualifier bits directly on `Type` nodes instead of
discarding them during parsing.

Covered behavior:

```text
const / volatile / restrict / _Atomic are recorded on the parsed type
typedef-qualified base types preserve their qualifiers through cloning
qualified pointer layers preserve qualifier bits independently from the pointee
casts, typedefs, sizeof/alignof type names, and ordinary declarations share the same qualifier-aware type parser
```

Regression coverage:

```text
tests/type_semantics/test_qualified_pointer_typedef_parse.c
```

Validation note:

```text
After the Type layout change, a clean rebuild is required because the current
make setup does not fully track header dependencies for all objects.
Confirmed after clean rebuild:
  make build/tcc
  make test-types   -> 108/108 completed, 0 failed
  make stage0       -> 1056/1056 completed, 0 failed
```

### Modifiable-lvalue diagnostics

The frontend now rejects a broader class of non-modifiable lvalues instead of
silently accepting them or collapsing them into a generic "assignable" error.

Covered behavior:

```text
const-qualified locals reject assignment
const-qualified dereference results reject ++/--
const-qualified struct members reject assignment
array objects reject assignment with a modifiable-lvalue diagnostic
small typed locals preserve their real Type metadata instead of dropping qualifiers
```

Regression coverage:

```text
tests/errors/assign_const_local.c
tests/errors/inc_const_pointer_deref.c
tests/errors/assign_array_object.c
tests/errors/assign_const_struct_member.c
```

### `#warning` directive validation

The preprocessor warning path is now explicitly validated for both plain and
spaced-hash forms.

Covered behavior:

```text
#warning emits a non-fatal warning by default
-w suppresses the warning output
-Werror promotes the warning to a fatal error
# warning (with whitespace after #) behaves the same way
```

Regression coverage:

```text
tests/preproc/test_warning_directive.c
tests/preproc/test_warning_directive_spaced.c
```

### Macro-expanded `#include` validation

Macro-expanded include operands are implemented and explicitly covered for both
quoted and system-header forms.

Covered behavior:

```text
#define HEADER "my_header.h"
#include HEADER

#define SYSTEM_HEADER <stdbool.h>
#include SYSTEM_HEADER
```

Regression coverage:

```text
tests/preproc/test_include_macro_expansion.c
tests/preproc/test_include_macro_expansion.h
```

### Broader pragma coverage

The preprocessor now preserves and restores macro definitions across
`#pragma push_macro` / `#pragma pop_macro`, including `_Pragma(...)` form.

Covered behavior:

```text
direct #pragma push_macro("name") saves the current macro definition
direct #pragma pop_macro("name") restores the most recent saved definition
pushing an undefined macro and later popping it returns it to undefined
_Pragma("push_macro(...)") and _Pragma("pop_macro(...)") behave the same way
named #pragma pack(push, id, n) / pack(pop, id) restore the selected pack frame
`#pragma pack(pop, n)` restores the previous frame and then applies the new pack
`#pragma pack(pop, id, n)` restores the named frame and then applies the new pack
_Pragma("pack(push, id, n)") and _Pragma("pack(pop, id)") match direct pragma behavior
_Pragma("pack(pop, id, n)") matches the direct pragma variants
malformed `#pragma pack(...)` forms now fail instead of being silently accepted
invalid `#pragma pack(n)` values now fail instead of being accepted
unsupported pragmas now warn instead of being silently ignored
```

Regression coverage:

```text
tests/preproc/test_pragma_pack_invalid_value.c
tests/preproc/test_pragma_pack_malformed_invalid_arg.c
tests/preproc/test_pragma_pack_malformed_missing_comma.c
tests/preproc/test_pragma_pack_malformed_trailing_tokens.c
tests/preproc/test_pragma_push_pop_macro.c
tests/preproc/test_unknown_pragma_operator_warn.c
tests/preproc/test_unknown_pragma_warn.c
tests/structs/test_pack_named_push_pop.c
tests/structs/test_pack_pop_variants.c
tests/torture/00206.c
```

### Broader function-pointer compatibility

Function designators now preserve their real return type through assignment,
argument, return, and initializer compatibility checks instead of falling back
to a fake `int (*)()` shape.

Covered behavior:

```text
named function designators preserve recorded return-type metadata
function-pointer assignment rejects incompatible return types
function-pointer arguments reject incompatible return types
function-pointer returns reject incompatible return types
function-pointer initializers reject incompatible return types
empty () function declarators preserve old-style no-prototype meaning instead of collapsing to (void)
old-style function pointers interoperate with prototype-bearing ones when parameter types are stable under default argument promotions
old-style function pointers still reject prototype-bearing char/short/float-style mismatches that rely on promotions
repeated function declarations/definitions now apply the same old-style vs prototype compatibility rules
named function parameters such as int f(int g()) now parse as adjusted function-pointer parameters
ordinary typedef declarators now accept array and function forms such as typedef int arr_t[3]; and typedef int fn_t(void);
typedef-named function parameters adjust correctly to function pointers and interoperate with equivalent explicit pointer spellings in repeated declarations
functions returning old-style versus prototype-bearing function pointers now use the same compatibility rules during repeated declarations/definitions
compatible repeated declarations now remain stable across declaration-order reversals for typedef-named function parameters and returned function pointers
legacy GNU-ish void* <-> function-pointer compatibility remains accepted
parenthesized global pointer declarators like int (*ptr); keep ordinary pointer meaning
nested parenthesized function-pointer parameters such as int (**pp)(int) now parse correctly
calls through pointer-to-pointer-to-function expressions such as (*pp)(x) now keep the required data load instead of incorrectly calling pp itself
raw nested parenthesized declarators such as int (*(*pp))() now parse through the same compatibility path as equivalent typedef-based spellings
explicit nested returned-function-pointer prototypes such as int (*(*maker(void))(int))(int); now parse and preserve the real nested return type for later compatibility checks
explicit nested returned-function-pointer definitions such as int (*(*maker(void))(int))(int) { ... } now parse through the top-level function-definition path, and ordinary raw function-returning-function-pointer definitions now retain precise return-type metadata for downstream compatibility checks
explicit nested returned-function-pointer spellings now participate in reverse-order repeated declarations and reject incompatible inner function signatures
explicit nested function-pointer parameter spellings now participate in reverse-order repeated declarations and reject incompatible inner function signatures
local nested function-pointer declarations now preserve their real composite type instead of collapsing to a stub pointer-to-pointer shape
indirect calls through nested function-pointer chains now preserve function return metadata correctly, including torture-style cases such as calling through a local pointer to a function returning a function pointer
conditional expressions that merge old-style and prototype-bearing function pointers now keep the prototype-bearing composite type when the signatures are promotion-compatible
conditional expressions that merge pointers to old-style versus prototype-bearing function pointers now preserve deeper prototype metadata as well, so later dereference-and-call argument diagnostics still fire even when the old-style arm appears first
indirect calls through conditional or local function-pointer expressions now apply prototype-sensitive argument conversions and pointer-compatibility diagnostics instead of silently falling back to old-style call behavior
global/static function-pointer initializers now validate real function-designator compatibility instead of accepting every function name blindly
global arrays of function pointers now reject incompatible bare function-designator initializers and still accept compatible ones
global struct fields of function-pointer type now reject incompatible bare function-designator initializers and still accept compatible ones
block-scope struct fields of function-pointer type now reject incompatible bare function-designator initializers and still accept compatible ones
block-scope arrays of function-pointer type now accept compatible bare function-designator initializers and reject incompatible ones with the normal pointer-initializer diagnostic
block-scope one-dimensional arrays now accept designated function-pointer initializers such as [1] = fn and preserve zero-fill semantics for omitted elements
direct-spelling block-scope arrays such as int (*tab[1])(int) now parse and initialize through the same compatibility path as typedef-based spellings
direct-spelling file-scope arrays such as int (*tab[])(int) = { fn } now parse and initialize through the same relocation-backed compatibility path as typedef-based global spellings
direct-spelling static locals such as static int (*fp)(int) = fn now preserve their real function-pointer type and load through the hidden global correctly at call sites
direct-spelling nested static locals such as static int (*(*maker)(int))(int) = retfn now parse and initialize through the same hidden-global compatibility path as equivalent non-static locals
direct-spelling nested global and extern objects such as int (*(*maker)(int))(int) = retfn and extern int (*(*maker)(int))(int); now parse through the file-scope object declarator path instead of being misclassified as function declarations
direct-spelling block-scope static arrays such as static int (*tab[2])(int) now parse and initialize through hidden-global static-local lowering
typedef-named function pointers that return old-style versus prototype-bearing inner function pointers now reject incompatible global initializers and accept promotion-compatible ones
extern parenthesized global function-pointer declarators such as extern int (*fp)(int); now preserve their function-pointer type instead of collapsing to plain pointers-to-base-type
repeated global and extern object declarations now reject incompatible function-pointer object types instead of silently merging them
compatible global and extern object redeclarations now also upgrade the stored object type to the richer prototype-bearing composite form, so later calls keep prototype-sensitive diagnostics instead of sticking with the first weaker declaration
compatible function redeclarations that return old-style versus prototype-bearing function pointers now also preserve the richer stored return signature, so later indirect calls keep prototype-sensitive argument diagnostics instead of degrading to old-style call behavior
extern incomplete arrays such as extern int a[]; remain compatible with later sized definitions such as int a[2];
parenthesized pointer-to-incomplete-array declarations such as extern int (*pa)[]; now parse and remain compatible with later sized forms such as int (*pa)[2];
```

Regression coverage:

```text
tests/errors/assign_incompatible_function_pointer.c
tests/errors/assign_incompatible_function_pointer_oldstyle_char.c
tests/errors/call_incompatible_function_pointer_arg.c
tests/errors/call_conditional_function_pointer_incompatible_arg.c
tests/errors/call_conditional_nested_function_pointer_incompatible_arg.c
tests/errors/call_global_function_pointer_redecl_upgrade_incompatible_arg.c
tests/errors/call_extern_function_pointer_redecl_upgrade_incompatible_arg.c
tests/errors/call_redecl_returning_function_pointer_upgrade_incompatible_arg.c
tests/errors/init_incompatible_nested_returned_function_pointer_oldstyle_char.c
tests/errors/return_incompatible_function_pointer.c
tests/errors/init_incompatible_function_pointer.c
tests/errors/init_incompatible_function_pointer_param.c
tests/errors/init_incompatible_global_function_pointer_array_char.c
tests/errors/init_incompatible_direct_global_function_pointer_array_char.c
tests/errors/init_incompatible_global_struct_function_pointer_char.c
tests/errors/init_incompatible_local_function_pointer_array_char.c
tests/errors/init_incompatible_local_function_pointer_array_designated_char.c
tests/errors/init_incompatible_direct_function_pointer_array_char.c
tests/errors/init_incompatible_static_local_function_pointer_direct_char.c
tests/errors/init_incompatible_static_local_function_pointer_array_direct_char.c
tests/errors/init_incompatible_static_local_nested_function_pointer_direct_char.c
tests/errors/init_incompatible_local_struct_function_pointer_char.c
tests/errors/init_incompatible_global_nested_function_pointer_object_char.c
tests/errors/conflicting_global_function_pointer_decl_char.c
tests/errors/conflicting_extern_function_pointer_decl_char.c
tests/errors/conflicting_extern_incomplete_pointer_to_array_bound.c
tests/errors/prototype_redecl_incompatible_oldstyle_char.c
tests/errors/prototype_redecl_nested_fnptr_param_incompatible_char.c
tests/errors/prototype_redecl_returning_nested_fnptr_incompatible_char.c
tests/errors/prototype_redecl_returning_oldstyle_fnptr_char.c
tests/type_semantics/test_function_param_named_function_decl.c
tests/type_semantics/test_conditional_function_pointer_oldstyle_proto_call.c
tests/type_semantics/test_function_pointer_oldstyle_proto_compatible.c
tests/type_semantics/test_global_function_pointer_array_compatible.c
tests/type_semantics/test_direct_global_function_pointer_array_compatible.c
tests/type_semantics/test_global_nested_function_pointer_object.c
tests/type_semantics/test_global_nested_returned_function_pointer_oldstyle_proto_compatible.c
tests/type_semantics/test_global_struct_function_pointer_compatible.c
tests/type_semantics/test_local_function_pointer_array_compatible.c
tests/type_semantics/test_local_function_pointer_array_designated_compatible.c
tests/type_semantics/test_direct_function_pointer_array_compatible.c
tests/type_semantics/test_local_struct_function_pointer_compatible.c
tests/type_semantics/test_static_local_function_pointer_direct_compatible.c
tests/type_semantics/test_static_local_function_pointer_array_direct_compatible.c
tests/type_semantics/test_static_local_nested_function_pointer_direct_compatible.c
tests/type_semantics/test_extern_incomplete_array_definition_compatible.c
tests/type_semantics/test_extern_incomplete_pointer_to_array_decl.c
tests/type_semantics/test_extern_nested_function_pointer_object.c
tests/type_semantics/test_function_redecl_oldstyle_void_compatible.c
tests/type_semantics/test_function_redecl_returning_oldstyle_fnptr_compatible.c
tests/type_semantics/test_function_redecl_returning_oldstyle_fnptr_reverse_order.c
tests/type_semantics/test_function_redecl_typedef_fnparam_reverse_order.c
tests/type_semantics/test_function_pointer_typedef_param.c
tests/type_semantics/test_function_redecl_nested_fnptr_param_explicit_compatible.c
tests/type_semantics/test_function_redecl_nested_fnptr_param_explicit_reverse_order.c
tests/type_semantics/test_function_redecl_returning_nested_fnptr_explicit_proto.c
tests/type_semantics/test_function_redecl_returning_nested_fnptr_explicit_reverse_order.c
tests/type_semantics/test_function_returns_nested_function_pointer_runtime.c
tests/type_semantics/test_typedef_array_decl.c
tests/type_semantics/test_typedef_function_decl_param.c
tests/type_semantics/test_global_parenthesized_pointer.c
tests/type_semantics/test_nested_function_pointer_param.c
tests/type_semantics/test_nested_function_pointer_param_oldstyle_proto_compatible.c
tests/torture/00124.c
tests/torture/00095.c
tests/torture/00209.c
```

### Nested pointer compatibility diagnostics

Nested object-pointer compatibility checks now reject incompatible pointed-to
types more consistently across the main frontend diagnostic sites.

Covered behavior:

```text
pointer-to-pointer assignment rejects incompatible pointee types
direct-call pointer arguments reject incompatible pointer-to-pointer types
local pointer initializers reject incompatible pointer-to-pointer types
return statements reject incompatible pointer-to-pointer types
static/global pointer initializers reject incompatible pointer-to-pointer types
```

Regression coverage:

```text
tests/errors/assign_incompatible_pointer_to_pointer.c
tests/errors/call_incompatible_pointer_to_pointer_arg.c
tests/errors/init_incompatible_pointer_to_pointer.c
tests/errors/return_incompatible_pointer_to_pointer.c
tests/errors/global_init_incompatible_pointer_to_pointer.c
```

### Pointer/integer implicit-conversion diagnostics

The main local frontend diagnostic sites now reject implicit pointer/integer
conversions in both directions instead of silently accepting them.

Covered behavior:

```text
pointer-to-integer assignment now fails
pointer-to-integer local initialization now fails
pointer-to-integer return now fails
pointer-to-integer fixed-parameter call arguments now fail
integer-to-pointer assignment now fails unless the integer is a null-pointer constant
integer-to-pointer local initialization now fails unless the integer is a null-pointer constant
integer-to-pointer return now fails unless the integer is a null-pointer constant
integer-to-pointer fixed-parameter call arguments now fail unless the integer is a null-pointer constant
```

Regression coverage:

```text
tests/errors/assign_incompatible_pointer_to_integer.c
tests/errors/init_incompatible_pointer_to_integer.c
tests/errors/return_incompatible_pointer_to_integer.c
tests/errors/call_incompatible_pointer_to_integer_arg.c
tests/errors/assign_incompatible_integer_to_pointer.c
tests/errors/init_incompatible_integer_to_pointer.c
tests/errors/return_incompatible_integer_to_pointer.c
tests/errors/call_incompatible_integer_to_pointer_arg.c
```

### Local pointer-to-array declarators

Block-scope pointer-to-array declarators now parse through the ordinary local
declaration path instead of being misclassified as function-pointer declarations.

Covered behavior:

```text
local declarations such as int (*pa)[4] now parse correctly
local pointer-to-array initializers such as int (*pa)[4] = &values now compile
incompatible local pointer-to-array initializers now fail with the normal pointer initializer diagnostic
```

Regression coverage:

```text
tests/type_semantics/test_local_pointer_to_array_decl.c
tests/errors/init_incompatible_local_pointer_to_array.c
```

### Functions returning pointer-to-array types

File-scope declarations and definitions that return pointer-to-array types now
parse through the dedicated returned-pointer declarator path instead of falling
back to plain function-name parsing.

Covered behavior:

```text
prototypes such as int (*get_values(void))[4]; now parse correctly
definitions such as int (*get_values(void))[4] { ... } now compile
incompatible repeated declarations returning different pointer-to-array types now fail
```

Regression coverage:

```text
tests/type_semantics/test_function_returns_pointer_to_array.c
tests/errors/prototype_redecl_returning_pointer_to_array_incompatible.c
```

### First-class `void`, `_Bool`, `enum`, and `union` typing

The frontend now carries `void`, `_Bool`, `enum`, and `union` through the type system
instead of collapsing them immediately to integer placeholders.

Covered behavior:

```text
void object/parameter/deref/arithmetic misuse is rejected
_Bool assignments and initializers canonicalize to 0 or 1
enum declarations preserve enum type identity through typedef/local declarators
enum local pointers and arrays now parse through the normal declaration path
union declarations preserve union type identity through typedef/local declarators
union local pointers and arrays now preserve aggregate typing
```

Regression coverage:

```text
tests/errors/sizeof_void.c
tests/errors/alignof_void.c
tests/errors/void_object.c
tests/errors/void_param.c
tests/errors/void_deref.c
tests/errors/void_ptr_arith.c
tests/type_semantics/test_bool_init_canonicalizes.c
tests/type_semantics/test_bool_assign_canonicalizes.c
tests/type_semantics/test_bool_zero_stays_zero.c
tests/type_semantics/test_enum_local_array_decl.c
tests/type_semantics/test_enum_local_pointer_decl.c
tests/type_semantics/test_union_local_array_decl.c
tests/type_semantics/test_union_local_pointer_decl.c
```

### Test harness manifest filtering

The test harness now supports substring filtering through `--filter`, allowing focused runs directly from the main manifest without creating temporary one-entry manifest files.

Example:

```sh
TCC_TEST_FLAGS="-boot" tests/:runtests.sh \
  -c build/tcc \
  -t arm64 \
  -a clang \
  -m tests/manifest.txt \
  -D build/tmp \
  --filter test_directive_space_after_hash
```

This is now useful for focused frontend, preprocessor, and backend regression checks while keeping the canonical manifest as the source of truth.

### Preprocessor spaced-hash directive coverage

The preprocessor now accepts whitespace between `#` and directive names, including nested variants and `# include`.

Current focused group:

```text
test_directive_space_after_hash*: 6/6 passed
```

Coverage includes:

```text
# define
# include
# error
# if
# ifdef
# ifndef
# undef
# else
# endif
defined(...)
nested spaced directives
macro expansion through spaced nested directives
multiple spaces after #
```

Key files:

```text
tests/preproc/test_directive_space_after_hash.c
tests/preproc/test_directive_space_after_hash_variants.c
tests/preproc/test_directive_space_after_hash_include.c
tests/preproc/test_directive_space_after_hash_include.h
tests/preproc/test_directive_space_after_hash_error.c
tests/preproc/test_directive_space_after_hash_defined.c
tests/preproc/test_directive_space_after_hash_nested_macro.c
```

### `#if` / `#elif` macro expansion fixed

A real preprocessor correctness issue was found and fixed: `#if` and `#elif` expressions were being evaluated before macro expansion.

Failing pattern before the fix:

```c
# define BASE_VALUE 10
# define SELECTED_VALUE BASE_VALUE

# if SELECTED_VALUE == 10
int selected(void) { return 42; }
# else
int selected(void) { return 1; }
# endif
```

The evaluator now macro-expands expressions before evaluation while preserving the special `defined(...)` operator.

Regression:

```text
tests/preproc/test_if_macro_expansion.c
manifest entry: run:preproc/test_if_macro_expansion.c::0
```

Confirmed focused result:

```text
preproc: .
1/1 Completed
0 Failed
```

Confirmed full-suite result:

```text
make stage0: 1054/1054 completed, 0 failed
```

### x64 ABI hardening

The x64 execution smoke suite is now strong and covers many ABI-sensitive cases.

Recent additions include:

```text
x64_varargs_many.c
x64_stack_alignment_call.c
x64_struct_arg_large.c
```

These cover:

```text
mixed variadic arguments crossing register/stack boundaries
nested calls with stack-passed arguments and local stack usage
large by-value struct arguments
stack alignment pressure
```

Confirmed:

```text
x64 execution smoke test: passed
```

Additional currently confirmed backend/runtime status:

```text
x86 assembly smoke test:   passed
MIPS assembly smoke test:  passed
m68k assembly smoke test:  passed
debug section smoke test:  passed
x64 execution smoke test:  passed
x86 execution smoke test:  skipped on this host (clang -m32 unavailable)
```

### x86 cdecl ABI smoke expansion

The x86 assembly smoke suite now includes broad cdecl ABI coverage:

```text
x86_many_args.c
x86_i64_args_return_mix.c
x86_varargs_many.c
x86_stack_alignment_call.c
x86_struct_arg_large.c
x86_struct_return_with_args.c
x86_nested_struct_return_call.c
```

These cover:

```text
many scalar stack arguments
mixed int / pointer / long long argument layouts
mixed variadic argument layout
nested cdecl call cleanup with stack-passed arguments and locals
hidden struct-return buffer handling combined with normal scalar arguments
struct-return results consumed through explicit temporaries
byte-copy by-value struct arguments on the outgoing cdecl stack
```

Current limitation:

```text
x86 execution smoke test:
  SKIP host cannot compile/link 32-bit x86 binaries with clang -m32
```

This is an environment limitation on the current macOS host, not a current regression.

### Direct struct-return call used as by-value struct argument

The frontend accepts direct struct-return function calls in by-value struct argument positions.

Example:

```c
return consume_pair_nested(make_pair_nested(10, 20), 3);
```

Regression:

```text
tests/structs/test_struct_return_direct_arg.c
manifest entry: run:structs/test_struct_return_direct_arg.c::55
```

Confirmed through full `make test`.

## Source-base completeness review

This section records the current state of the compiler against the C language and compiler implementation areas.

### Overall conclusion

The compiler is a strong small compiler for a substantial integer/pointer/aggregate C subset, but it should not yet claim full ISO C89/C99/C11/C17/C23 conformance.

Best current description:

```text
Freestanding C89/C99-style integer-and-basic-floating subset
+ selected C11/C23 syntax gates
+ GNU-ish extensions where useful
+ multi-target assembly generation
+ useful local optimizations
- incomplete hosted C library
- incomplete full type semantics
- incomplete floating-point semantics/ABI coverage
- incomplete full ISO preprocessor edge cases
```

### Preprocessor completeness

Implemented or mostly implemented:

```text
#define object-like macros
#define function-like macros
#undef
#ifdef / #ifndef
#if / #elif / #else / #endif
defined(...)
macro expansion in #if / #elif expressions
#include "file"
#include <system>
#error
#line
#warning recognition
#warning non-fatal warning behavior
#include MACRO_NAME with quoted or system-header expansion
#pragma pack(...)
backslash-newline splicing
comment stripping
object/function macro recursion blocking
variadic macros
__VA_ARGS__
stringification #
token pasting ##
GNU comma-swallowing with ##__VA_ARGS__
__FILE__
__LINE__
__DATE__
__TIME__
target macros such as __x86_64__ and __aarch64__
spaced hash directives such as # define and # include
```

Preprocessor gaps:

```text
_Pragma currently has focused pack(...) coverage rather than broad pragma coverage.
Standard predefined macro support exists but remains incomplete versus the full implementation-defined macro surface.
Trigraph support is intentionally limited.
Digraph token support and UCN support exist, but the unsupported/partial edges should be documented more explicitly.
Full pp-number edge cases are incomplete.
System include behavior is intentionally stub-like.
```

Recommended preprocessor next steps:

```text
1. Add explicit tests/documentation for trigraph/digraph/UCN decisions, even where behavior is intentionally partial.
2. Broaden _Pragma coverage if new supported pragmas are added beyond pack(...).
3. Expand implementation-defined predefined macro coverage only as new language/runtime features require it.
4. Continue tightening pp-number and system-include edge cases.
```

### Lexer completeness

Implemented or partially implemented:

```text
identifiers
integer constants: decimal, octal, hex
basic integer suffix handling: u/U, l/L, ll-style sizing
character literals
multi-character literal warning
string literals
adjacent string literal concatenation
prefixed strings/chars: L, u, U, u8 partial support
operators needed by current parser
keywords for core C constructs
source locations and #line-aware logical locations
```

Lexer gaps:

```text
Floating literals now exist for the currently supported decimal float/double paths, but lexical/semantic coverage is still partial.
Hex floating constants are incomplete.
Universal character names are partially supported for identifiers; literal coverage remains incomplete.
Full escape-sequence semantics are incomplete.
auto and register are tokenized and covered for block/file scope.
Thread-local storage-class spelling is diagnosed but not implemented.
Complex/imaginary numeric tokens are missing.
C23 lexical additions are incomplete.
```

### Parser and language feature completeness

Implemented or partially implemented:

```text
functions
function prototypes
function calls
recursion
local variables
global variables
static locals
extern globals/functions
typedef
struct
union
enum
pointers
arrays
array indexing
address-of / dereference
member access . and ->
assignment
compound assignment
pre/post increment and decrement
arithmetic operators
bitwise operators
logical operators
comparisons
conditional operator ?:
comma expression
casts
sizeof(type)
sizeof(expr)
if / else
while
do / while
for
break / continue
switch / case / default
goto / labels
return
string literals
global initializers
local initializers
designated initializers
compound literals
flexible array members
function pointers
variadic functions through project stdarg support
_Generic simple support
__func__
inline keyword accepted
const / volatile / restrict / _Atomic tokens accepted in places
asm / __asm__ extension
#pragma pack support path
```

Major language gaps:

```text
Floating-point support is partial rather than absent.
  float and double now have real size/alignment identity.
  current decimal literals, simple expressions/casts, global initializers, and the validated arm64/x64/self-host paths now work for the existing suite.
  broader ISO semantics, hex floats, and full backend ABI coverage remain incomplete.

No real qualifier semantics.
  const is not enforced.
  volatile does not force memory side-effect behavior.
  restrict has no aliasing semantics.
  _Atomic is gated by standard mode but not semantically implemented.

The type system is still small.
  Core TypeKind coverage appears centered around:
    TY_INT
    TY_CHAR
    TY_SHORT
    TY_PTR
    TY_ARRAY
    TY_STRUCT

  Missing or incomplete as first-class semantic types:
    TY_ATOMIC

Function pointee types now exist, but full ISO function-type compatibility and
prototype equivalence rules are still incomplete.
Nested function-pointer parameters, returned-function-pointer spellings, local
nested function-pointer declarations, and the main indirect-call runtime chains
are now covered, so the remaining gaps are narrower than before and are
primarily deeper prototype-equivalence, compound compatibility, and diagnostic
corner cases rather than basic declarator parsing.
Incomplete type rules are partial.
Bit-fields and runtime VLAs now have working implementation coverage, but full ISO edge-case completeness is still incomplete.
Compound type compatibility is not close to full ISO rules.
Qualifier semantics are still incomplete beyond the currently enforced const-style cases.
Usual arithmetic conversions are partial.
Integer promotions are partial.
Pointer compatibility and constraint diagnostics are incomplete.
Broader scalar implicit-conversion diagnostics are still incomplete beyond the
now-enforced pointer/integer local assignment, initializer, return, and
fixed-parameter call cases.
Modifiable-lvalue checks are incomplete.
Initializer conformance now covers more aggregate designator, brace-elision,
zero-fill, and excess-initializer cases; full aggregate edge-case coverage is
still incomplete.
Full C23 feature set is not implemented.
```

### Standard mode completeness

Accepted modes include:

```text
-std=c89
-std=c90
-std=c99
-std=c11
-std=c17
-std=c23
```

These currently act as feature gates and diagnostics modes, not full conformance modes.

Implemented standard-gate behavior includes:

```text
C89 rejects // comments
C89 rejects mixed declarations/statements
C89 rejects compound literals
C89 rejects designated initializers
C89 rejects variadic macros
C89 rejects long long
C89 rejects inline
C89 rejects _Bool
C89 rejects restrict
C89 rejects _Atomic
C99 rejects _Atomic before C11
C99 rejects _Generic before C11
C99+ rejects implicit function declarations
```

Recommendation:

```text
Document -std as selected feature gating only. Do not claim full C23 conformance yet.
```

### Standard library / hosted C completeness

The project-local headers are useful bootstrap/test stubs, not a full hosted C implementation.

Existing stub or partial headers include:

```text
stdio.h
stdlib.h
string.h
stdint.h
stddef.h
stdarg.h
stdbool.h
ctype.h
errno.h
math.h
time.h
unistd.h
wchar.h
sys/wait.h
sys/resource.h
```

Major hosted C gaps:

```text
Full libc implementation
Full errno semantics
Full stdio behavior
Full math library behavior
locale.h
setjmp.h
signal.h
assert.h
limits.h completeness
float.h correctness
inttypes.h
threads.h
stdatomic.h
uchar.h
complex.h
fenv.h
tgmath.h
wctype.h
```

Recommended wording:

```text
Freestanding/bootstrap headers, not a conforming hosted C library.
```

### Optimizer completeness

Implemented optimizations include:

```text
AST constant folding
AST dead-code elimination after terminal statements
IR dead-store elimination
IR pure dead-value elimination
IR label cleanup
IR redundant store/load forwarding
IR copy propagation
IR local constant propagation
IR constant folding
IR unary folding
identity peepholes
mul-by-power-of-two to shift
branch folding
jump-to-next-label removal
linear unreachable-code removal
```

Missing larger optimizer classes:

```text
No SSA
No global data-flow analysis
No production register allocator
No common subexpression elimination
No loop-invariant code motion
No general strength reduction beyond simple peepholes
No alias analysis
No interprocedural optimization
No inlining
No global constant propagation
No target-specific instruction scheduling
```

Recommendation:

```text
Treat -O1/-O2 as local optimization levels, not production optimization levels.
```

## Backend coverage summary

### arm64

arm64 remains the primary fully exercised native target on the current macOS host.

Status:

```text
stage0 arm64: 1150/1150 passed
stage1 arm64: not revalidated in this pass
stage2 bootstrap build: currently failing in cc/ir.c self-host compilation on this host
stress/backend smoke: last previously confirmed PASS
```

Recommendation:

```text
Continue treating arm64 as the correctness baseline for frontend, type system,
debug-info, bootstrap, and manifest validation.
```

### x64

x64 is mature for both assembly-generation and native execution smoke coverage.

Current coverage includes:

```text
scalar calls and argument lowering
register and stack argument paths
stack-passed overflow arguments
nested calls with local stack usage
stack-alignment-sensitive call paths
globals and global initializers
static locals
recursive, nested, and mutual calls
function pointers and indirect calls
struct field access
small, medium, and large struct arguments
struct returns
memory-passed aggregate arguments larger than 16 bytes
64-bit arithmetic, comparisons, shifts, and bitwise operations
casts
varargs including register-to-stack spillover
control-flow constructs
debug section emission
```

Recommendation:

```text
Keep x64 as the runtime cross-check target. Add focused x64 execution tests
whenever frontend semantics are expanded.
```

### x86

x86 assembly coverage is now strong and close to x64 in breadth.

Current limitation:

```text
x86 execution smoke test:
  SKIP host cannot compile/link 32-bit x86 binaries with clang -m32
```

Current ABI state:

```text
x86 by-value struct arguments now use native cdecl-style stack byte copies
instead of the earlier pointer-lowered internal calling convention.
```

Recommendation:

```text
Treat x86 runtime execution as an environment limitation for now. Keep assembly
smoke coverage green and expand runtime validation when a usable 32-bit host
or emulator path is available.
```

### MIPS

MIPS assembly smoke coverage is broad and green.

Current limitation:

```text
MIPS execution smoke test:
  SKIP missing MIPS cross compiler/toolchain on this host
```

Recommendation:

```text
Add a host-optional MIPS execution smoke harness using qemu-mips/qemu-mipsel
and a MIPS cross compiler where available. Start with simple arithmetic, call,
global, pointer, and control-flow tests before aggregate-heavy tests.
```

### m68k

m68k assembly smoke coverage is useful and green.

Current limitation:

```text
m68k execution smoke test:
  SKIP missing m68k emulator/toolchain
```

Recommendation:

```text
Expand the new host-optional m68k execution harness when an emulator/toolchain
is available. Keep the initial runtime subset conservative, then add more
aggregate-heavy and ABI-sensitive cases after the basic call/global/pointer/
control-flow cases are stable.
```

## Completeness estimate

Approximate current state:

```text
Preprocessor:        60-70% of common C needs, lower for strict ISO edge cases
Lexer:               55-65%
Parser syntax:       55-65% for common integer C, much lower for full ISO C
Type semantics:      30-40%
Integer codegen:     65-75% on arm64/x64 for supported subset
Floating codegen:    partial and improving, but not yet broad ISO coverage
Hosted libc:         10-20%
Optimizer:           20-30% of production expectations
Debug info:          useful scaffold, not complete DWARF
Full ISO C:          not yet close
```

## Recommended roadmap

### Phase 1: Document the supported language

Create `docs/SUPPORTED_C.md` with:

```text
Supported
Partially supported
Not supported
```

Declare the current target as:

```text
A freestanding C89/C99-style integer subset with selected C11 syntax.
```

Avoid claiming C23 conformance even though `-std=c23` is accepted.

### Phase 2: Finish preprocessor correctness

Highest-value tasks:

```text
1. Decide and document unsupported trigraph/digraph/UCN behavior more explicitly.
2. Broaden standard predefined macro coverage if more dialect-specific macros are added later.
```

### Phase 3: Improve type-system correctness

Before adding big new syntax, strengthen semantic foundations:

```text
qualifier tracking on Type
modifiable-lvalue checks
better pointer compatibility checks
bit-field parsing decision
```

Completed in this phase so far:

```text
qualifier tracking on Type
modifiable-lvalue checks
bit-field parsing/representation work
basic pointer assignment compatibility checks
basic direct-call pointer argument compatibility checks
basic pointer return compatibility checks
basic local pointer initializer compatibility checks
basic static/global pointer initializer compatibility checks
basic conditional-expression pointer compatibility checks
basic pointer comparison compatibility checks
basic pointer-returning call compatibility checks
broader typed object-pointer expression compatibility checks
broader nested pointer-to-pointer compatibility diagnostics
broader function-pointer compatibility checks based on preserved function-designator return types
local pointer-to-array declarator parsing and initializer diagnostics
functions returning pointer-to-array declarators and redeclaration checks
nested returned-function-pointer typedef compatibility across argument-passing, local-initializer, conditional, and equality-comparison paths
nested function-pointer-parameter typedef compatibility across local-initializer, conditional, and equality-comparison paths
nested variadic function-pointer-parameter compatibility across direct-call, local-initializer, conditional, equality-comparison, and redeclaration paths
nested variadic returned-function-pointer compatibility across direct-call, argument-passing, local-initializer, conditional, equality-comparison, and redeclaration paths
```

Primary remaining semantic task:

```text
broaden pointer and prototype compatibility checks beyond the current assignment/direct-call/return/local-initializer/static-global-initializer/conditional/comparison/pointer-returning-call/object-pointer-expression/function-pointer-return-type/prototype-sensitive function-pointer/old-style-vs-prototype function-pointer/repeated-declaration/nested declarator/nested typedef-based compatibility/nested variadic function-pointer coverage, especially for deeper prototype-equivalence, compound compatibility, and stricter diagnostic corner cases that are still intentionally loose
```

### Phase 4: Complete floating point deliberately

Floating point is a major feature, not a small patch.

Minimum plan:

```text
finish lexer coverage for broader floating literal forms
broaden AST/type-system floating semantics beyond the current validated subset
complete IR coverage for arithmetic, comparisons, and casts
broaden backend ABI handling beyond the currently validated arm64/x64/self-host paths
add wider regression coverage for arithmetic, conversions, comparisons, varargs, and aggregate ABI edge cases
```

### Phase 5: Execution validation for non-x64 backends

Next backend validation target:

```text
broaden host-optional MIPS and m68k execution subsets beyond the current conservative smoke coverage
```

### Phase 6: Validate x86 aggregate ABI further

The x86 frontend/backend path now uses byte-copy by-value aggregate passing for
struct arguments. The remaining work here is validation breadth, not ABI policy.

## Recommended next priorities

### Priority 1: Commit the current green state

Before starting the next semantic slice, checkpoint the currently validated
frontend, smoke-test, and cleanup-rule work in a single up-to-date commit.

### Priority 2: Broaden non-x64 execution coverage

The MIPS and m68k runtime paths now exist as host-optional smoke harnesses.
The next step is to widen those subsets gradually on hosts with QEMU/cross tools
installed, especially around aggregate passing/returns, varargs, and other
ABI-sensitive cases.

### Priority 3: Type-system foundations

After the next preprocessor/harness slice, begin moving the type system toward standard C by adding first-class missing types and better diagnostics.

Highest value order:

```text
broaden pointer/prototype compatibility checks beyond the current assignment/direct-call/return/local-initializer/static-global-initializer/conditional/comparison/pointer-returning-call/nested typedef-based and nested variadic function-pointer coverage
```

## Open gaps

Known language/conformance gaps:

```text
Full ISO C conformance is not claimed.
Floating-point semantics/codegen are present but still partial.
Full hosted C library is missing.
Type compatibility and qualifier semantics are partial.
_Pragma exists, but broader pragma coverage remains limited.
Bit-fields and runtime VLAs exist but remain incomplete versus full ISO edge cases.
Implementation-defined predefined macro coverage is still incomplete.
```

Known validation gaps:

```text
x86 native execution unavailable on current macOS host due to missing 32-bit link/runtime support.
MIPS execution smoke harness is host-optional and currently skips on this host due to missing cross tools.
m68k execution smoke harness is host-optional and currently skips on this host due to missing cross tools.
```

Known backend ABI state:

```text
x86 by-value struct arguments now use cdecl-style stack byte copies.
```

## Suggested immediate command sequence

```sh
git status --short
make stage2
make test
git add cc/ tests/ Makefile Recommendations.MD
git commit -m "Refresh frontend validation and smoke coverage"
```

Then start the next small fix:

```text
broaden pointer compatibility checks beyond the current assignment/direct-call/return/local-initializer/conditional/comparison/pointer-returning-call coverage
```


The two unresolved items from last review:

long_value is still long in both Node and Token. Still truncates 64-bit literals on ILP32 targets.
FuncTypeMeta list is never freed. func_type_meta_head in ast.c accumulates across the entire compilation unit. free_ast() doesn't touch it, parser_reset() doesn't touch it. For single-shot compilation this is harmless. Add a ast_func_type_meta_reset() called from parser_reset() when/if library-mode reuse is ever needed.

One new item to note:

No diagnostic warnings in parser/expr/stmt. The type-checking predicates (type_pointer_assignment_compatible, type_equal_unqualified) are used to guard branches, but the mismatch branches either silently succeed or hard-error. tcc_warn() is available and tcc_warnings_enabled() / tcc_warnings_as_errors_enabled() are exposed, but no warning calls have been added to the new type-checking sites yet. The infrastructure is ready; the call sites aren't wired up.


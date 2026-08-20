# C99/C11 Punch List

This document is the strict remaining-work list for reaching a defensible
"100%" freestanding ISO C99/C11" claim for the current compiler.

Current verified baseline:

```text
make stage2: pass
make test: pass, stage0 10047/10047 and stage1 10047/10047
make test-conformance-c99: pass, 2290/2290
make test-conformance-c11: pass, 2302/2302
make test-conformance-c17: pass, 2302/2302
make test-conformance-c23: pass, 2303/2303
make test-conformance-external-torture: pass, 220/220
make test-sanitize: pass, 10039/10039 under ASAN+UBSAN
make test-installed-smoke: pass
make test-release-gates: pass
sqlite smoke on Darwin arm64: pass
```

Interpretation:

- The curated in-tree C99/C11 baseline is currently clean.
- That does not yet justify a "100%" C99/C11 claim.
- This punch list is therefore the delta between "clean current baseline" and
  "defensible full freestanding C99/C11 claim".

It is derived from the still-`Partial` and `Unsupported` rows in
`docs/C99_C11_CONFORMANCE_MATRIX.md`, plus the production requirements already
tracked in `docs/ISO_C_PRODUCTION_PLAN.md`.

The categories below are intentionally pragmatic:

- `Test-only`: behavior appears implemented enough for the claimed surface, but
  the standards matrix still needs explicit coverage.
- `Diagnostics-only`: parser/semantic behavior mostly exists, but invalid
  program constraints still need explicit standards-gated diagnostics or
  broader invalid-input coverage.
- `Implementation`: real front-end / semantic / IR / codegen work remains.
- `Major blocker`: a gap that prevents an honest "100%" C99/C11 claim even if
  many other partial rows are just missing breadth or coverage.

## Exit Criteria

Before claiming "100%" freestanding C99/C11, all of the following should be
true:

- No `Unsupported` rows remain for the claimed C99/C11 language target.
- No `Partial` rows remain where real semantics are still missing rather than
  just needing breadth tests.
- Every remaining standards rule we claim has explicit regression coverage.
- External conformance and hardening baselines are strong enough to defend a
  production-grade claim.

## Rank 1: Major Blockers

- `_Complex` / `_Imaginary`
  Classification: `Major blocker`
  Why: `_Complex` object types, typedefs, struct/union members, `sizeof`, and
  C11 `_Alignof` type-name coverage now work, the compiler no longer
  advertises `__STDC_NO_COMPLEX__`, explicit real-scalar to `_Complex`
  value casts plus `_Complex`-to-real-scalar and `_Complex`-to-`_Complex`
  conversions now materialize correct values, `_Generic` association
  matching now works for the covered `_Complex` object types, both local /
  expression and file-scope/static-storage compound literals now work in the
  covered cases, same-type arithmetic now works in the covered float/double
  cases, pointer-to-`_Complex` function signatures are now accepted in the
  covered declaration/definition cases, and arm64 by-value `_Complex float` /
  `_Complex double` call-and-return behavior is now regression-tested for both
  same-translation-unit and external-call paths. `_Imaginary` parsing,
  conversions, arithmetic, comparisons, compound literals, and direct and
  indirect calls now have focused float/double/long-double coverage. `_Bool`
  conversion now observes both complex components and the imaginary component
  across casts, initialization, assignment, arguments, and returns, including
  direct complex-return call results. Complex and imaginary controlling
  expressions now also have focused coverage for unary `!`, short-circuit
  `&&` / `||`, `if`, all loop forms, and `?:`. The
  remaining blocker is broader non-exercised promotion/conversion behavior and
  cross-target by-value `_Complex` / `_Imaginary` ABI coverage, so full C99/C11
  language conformance is not yet claimable.

- Floating types and expressions beyond the currently validated arm64 subset
  Classification: `Major blocker`
  Why: `float`/`double` semantics, ABI handling, and cross-target behavior are
  still only partially claimed. The current arm64 coverage is strong, but full
  C99/C11 conformance cannot depend on a narrow target subset.

- External conformance breadth and production hardening
  Classification: `Major blocker`
  Why: even if the matrix rows become `Supported`, the current claim is not
  strong enough without broader external inputs, sanitizer coverage, and
  fuzzing. This is a blocker for the phrase "production-grade" even if it is
  not a language-feature blocker.

## Rank 2: Real Implementation Work

- `_Atomic`
  Classification: `Implementation`
  Remaining work: full lock-free / memory-order semantics and broader object,
  aggregate, and API behavior beyond the current macro-based validated surface.

- `float` and `double` types
  Classification: `Implementation`
  Remaining work: complete ABI and library-facing behavior outside the current
  tested arm64 cases.

- Floating expressions
  Classification: `Implementation`
  Remaining work: remove remaining unsupported lowering paths, reduce AST
  fallback dependence, and broaden expression/codegen coverage across targets.

- Pointer conversions
  Classification: `Implementation`
  Remaining work: finish the full composite-pointer and function-pointer rule
  matrix, not just the currently covered compatibility and qualifier cases.

- Qualifiers
  Classification: `Implementation`
  Remaining work: complete qualifier propagation and composite-type semantics
  across the full standards surface.

- Function prototypes
  Classification: `Implementation`
  Remaining work: finish the full ISO compatibility matrix for prototype and
  old-style interactions, not just the heavily tested current subset.

- Old-style definitions
  Classification: `Implementation`
  Remaining work: broaden compatibility and promotion handling, especially the
  less common mixed old-style/prototype edge cases.

- Function calls and returns
  Classification: `Implementation`
  Remaining work: complete remaining ABI behavior, promotion/compatibility
  cases, and broader cross-target validation.

- Structs and unions
  Classification: `Implementation`
  Remaining work: aggregate edge cases, broader layout constraints, and more
  complete initialization / member-behavior coverage.

- Bit-fields
  Classification: `Implementation`
  Remaining work: full signedness, layout, packing, and cross-target ABI
  behavior matrix.

- Arrays
  Classification: `Implementation`
  Remaining work: full multidimensional parameter decay and qualification
  matrix, plus remaining edge cases around declarators and initialization.

- VLAs and variably modified types
  Classification: `Implementation`
  Remaining work: broader VMT constraints, remaining parameter/local/file-scope
  rule interactions, and more complete VLA usage semantics.

- Initializers
  Classification: `Implementation`
  Remaining work: full aggregate edge-case matrix, especially nested, partial,
  designated, union, and constraint-driven invalid cases.

- Compound literals
  Classification: `Implementation`
  Remaining work: full storage-duration, lvalue, decay, nested aggregate, and
  edge-case standards surface.

- Conditional operator
  Classification: `Implementation`
  Remaining work: complete pointer, function-pointer, struct, `void`, and
  arithmetic composite-type semantics beyond the current focused subset.

- Casts
  Classification: `Implementation`
  Remaining work: broaden floating, qualifier, and function-pointer edge-case
  handling and diagnostics.

- `sizeof` and `_Alignof`
  Classification: `Implementation`
  Remaining work: complete type coverage, especially unusual incomplete,
  qualified, and edge-case type-name combinations.

- `_Alignas`
  Classification: `Implementation`
  Remaining work: broaden standards constraints and type-name surface beyond
  the current validated subset.

- `_Generic`
  Classification: `Implementation`
  Remaining work: finish compatible-type matching breadth, especially more
  pointer, array, function, qualifier, and atomic-equivalence cases.

- Enums
  Classification: `Implementation`
  Remaining work: broader diagnostics and corner cases around values,
  conversions, and declaration-specifier interactions.

- Flexible array members
  Classification: `Implementation`
  Remaining work: broader extension behavior and remaining constraint/usage
  surface.

## Rank 3: Diagnostics-Heavy Gaps

- Diagnostics for constraints
  Classification: `Diagnostics-only`
  Remaining work: systematically fill invalid-program cases required by the
  standard, especially where implementation already rejects but coverage is not
  explicit.

- Lvalues and modifiable lvalues
  Classification: `Diagnostics-only`
  Remaining work: finish the constraint matrix for what is and is not a
  modifiable lvalue.

- Statements
  Classification: `Diagnostics-only`
  Remaining work: broader invalid placement and grammar-boundary coverage.

- `goto` and labels
  Classification: `Diagnostics-only`
  Remaining work: broader scope/linkage/placement diagnostics beyond the
  currently covered VLA and duplicate/undefined-label cases.

- `switch`
  Classification: `Diagnostics-only`
  Remaining work: broader enum cases and more invalid case-expression
  coverage.

- Storage classes
  Classification: `Diagnostics-only`
  Remaining work: broaden invalid combinations and the remaining TLS misuse /
  redeclaration matrix.

- Function specifiers
  Classification: `Diagnostics-only`
  Remaining work: finish invalid placement and linkage-semantics coverage for
  `inline` and `_Noreturn`.

- `_Static_assert`
  Classification: `Diagnostics-only`
  Remaining work: more tricky grammar-boundary placements where the rules are
  mostly implemented already.

- `_Bool`
  Classification: `Diagnostics-only`
  Remaining work: low priority unless we want even more redundant permutation
  coverage beyond the currently pinned invalid type-specifier combinations.

- Integer promotions
  Classification: `Diagnostics-only`
  Remaining work: more invalid/constraint cases and standards-result-type
  breadth, even though much of the runtime behavior is already pinned.

- Usual arithmetic conversions
  Classification: `Diagnostics-only`
  Remaining work: broader invalid and mixed-edge diagnostics, plus result-type
  completeness.

## Rank 4: Coverage-Heavy Remaining Rows

- Macro-expanded `#include`
  Classification: `Test-only`
  Remaining work: broaden header-search and nesting validation around the
  behavior already implemented.

- `_Pragma`
  Classification: `Test-only`
  Remaining work: finish standards-focused coverage around the currently
  implemented forms and warning/diagnostic behavior.

- Comments and translation phases
  Classification: `Test-only`
  Remaining work: add more phase-order edge cases; recent splice/comment/
  directive regressions have improved this, but it is still not exhaustive.

- Identifiers
  Classification: `Test-only`
  Remaining work: broaden UCN and edge-case identifier use/diagnostic coverage.

- Integer constants
  Classification: `Test-only`
  Remaining work: more suffix/rank/boundary breadth, especially malformed and
  target-boundary cases.

- Character and string literals
  Classification: `Test-only`
  Remaining work: broaden literal-family edge cases and invalid spellings.

- Declarations and declarators
  Classification: `Test-only` plus `Implementation`
  Remaining work: most common cases are already covered heavily, but the row is
  still partial because the ISO matrix is huge and some nested edge cases still
  cross into real parser/semantic work.

- Scope
  Classification: `Test-only`
  Remaining work: broaden shadowing/linkage scope matrix now that many core
  cases are already covered.

- Linkage and tentative definitions
  Classification: `Test-only`
  Remaining work: finish the remaining linkage matrix breadth.

## Suggested Execution Order

1. Finish `Test-only` and `Diagnostics-only` rows first.
   Why: they are the cheapest path to converting many `Partial` rows into
   `Supported` without destabilizing the compiler.

2. Finish `_Generic`, `_Static_assert`, declarator breadth, VLA constraints,
   pointer/composite-type rules, and initializer/compound-literal edges.
   Why: these are the highest-value remaining semantic rows that are already
   mostly implemented.

3. Finish floating semantics and `_Atomic`.
   Why: these are larger implementation areas and are close to major-blocker
   territory for the production claim.

4. Implement `_Complex` / `_Imaginary`.
   Why: this is the clearest single language-feature blocker for a full C99/C11
   claim and should be handled deliberately, not piecemeal.

5. Broaden external conformance, sanitizer coverage, fuzzing, and installed
   environment checks.
   Why: this is required to move from "very strong self-hosted subset" to a
   production-grade ISO C claim.

## Immediate Must-Finish Set

If the question is "what still blocks 100% right now?", this is the shortest
honest list:

- Implement `_Complex` / `_Imaginary`.
- Close the remaining floating-semantics and ABI gaps beyond the currently
  validated arm64 subset.
- Finish the remaining real semantic rows:
  `_Atomic`, `_Generic`, pointer/composite-type rules, declarator edge cases,
  initializer / compound-literal edges, and VLA / variably modified type
  constraints.
- Finish the remaining diagnostics matrix where the compiler still has only
  partial invalid-program coverage.
- Add broader external conformance coverage plus sanitizer / fuzz / installed
  compiler hardening evidence.

## Practical Definition Of "100%"

For this project, "100%" should not mean "all current in-tree tests are green".
It should mean:

- No remaining `Unsupported` rows for the claimed C99/C11 language target.
- No remaining `Partial` rows whose notes hide meaningful unimplemented
  semantics.
- Explicit regression coverage for every claimed standards rule.
- External and hardening baselines that support the production claim.

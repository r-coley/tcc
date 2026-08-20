# ISO C Production Plan

This document defines the next validation and implementation track for moving
the compiler from a strong self-hosted subset to a production-grade ISO C
compiler.

The immediate target is C99/C11 conformance. C17 and C23 remain accepted
dialect modes, but they are not the first full production conformance target.
Selected C23 behavior is still tracked explicitly so it can be validated and
expanded without overstating support.

## Current Position

The compiler is currently self-hosting and the project regression suite passes
for both stage0 and stage1. Stage2 bootstrap equality and stress checks pass.

The `-std=` option is real: it selects a language mode used by the
preprocessor, parser, and semantic checks. It currently accepts:

```text
c89
c90
c99
c11
c17
c18
c23
c2x
iso9899:1990
iso9899:1999
iso9899:2011
iso9899:2017
iso9899:2018
iso9899:2024
```

Accepting a mode does not imply full ISO conformance for that standard,
especially for hosted-library behavior and the broader unclaimed C23 surface.

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

The current C23 subset explicitly includes reserved-identifier rejection for
`bool`, `true`, `false`, `nullptr`, `nullptr_t`, `static_assert`, `thread_local`,
`alignas`, and `alignof`, while the same spellings remain accepted before C23.

## Production Definition

For this project, "production-grade ISO C" means:

- A clearly claimed language target, starting with C99/C11.
- A documented supported/unsupported surface for that target.
- Regression tests for every claimed feature.
- External conformance and torture suites with known pass/fail baselines.
- No expected compiler crashes on invalid input; invalid programs produce
  diagnostics or intentional internal-compiler-error reports.
- Self-host stage1 remains the product compiler, and stage2 remains stable.
- Code-size and performance regressions are tracked so correctness work does
  not undo the assembly-quality improvements already made.

## Track 1: C99/C11 Conformance Matrix

Status: in place and actively maintained.

Continue using the matrix to enumerate each required C99/C11 language area and
mark it as supported, partially supported, unsupported, or untested.

Initial categories:

- Translation phases and preprocessing
- Tokens, identifiers, universal character names, literals
- Declarations and declarators
- Type compatibility and composite types
- Integer promotions and usual arithmetic conversions
- Pointer conversions and qualifiers
- Function declarations, prototypes, old-style definitions, and calls
- Scope, linkage, storage duration, tentative definitions
- Initializers, including aggregate and designated initializers
- Expressions, lvalues, casts, conditional operator, sequence points
- Statements, labels, switch, goto, loops
- Structs, unions, bit-fields, flexible array members
- Enums
- VLAs and variably modified types
- Floating types, conversions, expressions, and ABI behavior
- `_Bool`, `_Complex`, `_Imaginary`, `_Atomic`, and optional C11 features
- Predefined macros and standard-version macros
- Diagnostics required by constraints

Deliverable:

```text
docs/C99_C11_CONFORMANCE_MATRIX.md
docs/C99_C11_PUNCH_LIST.md
```

## Track 2: External Validation

Status: initial baseline in place via vendored `tests/torture`; installed
compiler smoke and release-gate targets are also in place.

The in-tree test suite is still primarily a regression suite, not a complete
conformance suite. Continue adding external test inputs and recording their
baselines.

Next order:

1. Add another external conformance suite beyond `tests/torture`.
   The harness now exposes `make test-conformance-external` so new suites can
   be plugged in by category or explicit manifest path without additional
   Makefile restructuring.
2. Add chibicc-style semantic tests where licensing and shape are acceptable.
3. Add GCC torture subsets that are meaningful for the supported target.
4. Keep sqlite amalgamation as an integration compile/run smoke test.
5. Broaden installed/bootstrap libc and header smoke coverage.
6. Keep the release-gate sequence maintained as named `make` targets.

Deliverables:

```text
make test-conformance-c99
make test-conformance-c11
make test-conformance-c17
make test-conformance-c23
make test-conformance-external
make test-conformance-external-torture
make test-installed-smoke
make test-sqlite-smoke
make test-release-gates-core
make test-release-gates-installed
make test-release-gates
make report-selfhost-func-sizes
make report-sqlite-func-sizes
make report-sqlite-stage-times
make report-release-metrics
docs/CONFORMANCE_BASELINE.md
```

## Track 3: Semantic And Codegen Gaps

The remaining highest-value implementation and validation gaps for C99/C11 are:

- Continue widening `_Generic` success and diagnostic coverage until the
  remaining obvious compatible-type and qualifier holes are gone.
- Continue `_Static_assert` placement/gating coverage at tricky grammar
  boundaries, especially where C11 rejects and C23 accepts.
- Complete declarator/declaration matrix coverage, especially nested,
  qualifier-heavy, and function-pointer-heavy forms.
- Expand variably modified type and VLA constraint coverage.
- Complete aggregate initializer, compound literal, and assignment edge cases.
- Tighten qualifier, composite type, pointer compatibility, and function
  compatibility rules against the matrix.
- Expand diagnostics for invalid programs required by C constraints.
- Complete `float` and `double` expression lowering, conversions, comparisons,
  calls, returns, and ABI behavior across broader cases and targets.
- Reduce normal C99/C11 dependence on AST fallback; strict IR should become a
  useful completeness signal, not just a debugging mode.
- Make backend unsupported paths explicit diagnostics or ICEs, never silent
  wrong-code paths.

## Track 4: Robustness And Hardening

Production-grade means the compiler should be difficult to crash accidentally.

Required hardening:

- Run ASAN/UBSAN builds of the compiler over the full test suite.
- Add parser/preprocessor fuzzing with minimized repro capture.
- Continue replacing raw `abort()`/`exit(1)` paths with `ICE`, `fatal_*`, or
  structured driver errors as appropriate.
- Keep guarded allocation and string helpers enabled in every compiler stage.
- Add tests for installed include discovery and clean-system-header behavior.

## Track 5: Release Gates

A production candidate must pass:

- Clean rebuild from scratch.
- `make stage2`.
- Full stage0 and stage1 regression tests.
- C99/C11 conformance baseline with no unexpected failures.
- sqlite compile/link/run smoke.
- Installed compiler smoke from outside the source tree.
- Function-size comparison gate against stage0 with an agreed threshold.
- Compiler timing gate for representative large inputs.
- Sanitizer run of the compiler test suite.

The build now exposes the currently runnable gate split as:

- `make test-release-gates-core`
- `make test-release-gates-installed`
- `make test-release-gates`

The remaining non-binary release metrics are now exposed as report targets:

- `make report-selfhost-func-sizes`
- `make report-sqlite-func-sizes`
- `make report-sqlite-stage-times`
- `make report-release-metrics`

`test-release-gates-installed` assumes `make install` has already been run and
that `SQLITE_SMOKE_DIR` points at a sqlite amalgamation tree.

## Execution Order

1. Finish the easy, low-risk C11 wins:
   `_Generic`, `_Static_assert`, qualifier-heavy declarators, and VLA
   constraint tests.
2. Broaden diagnostics coverage for features that already work in common
   cases.
3. Expand medium-risk semantic areas:
   initializers, compound literals, aggregate edges, linkage/inline semantics,
   and composite-type rules.
4. Broaden floating-point, aggregate ABI, `_Atomic`, and strict-IR coverage.
5. Add more external conformance inputs and keep a recorded baseline.
6. Only then treat the compiler as approaching “production-grade ISO C”.

## Immediate Next Steps

1. Continue adding small, verified C11 tests in areas already supported by the
   compiler, prioritizing `_Generic`, `_Static_assert`, declarator, and VLA
   constraint coverage.
2. Keep the explicit `-std=c23` subset measurable via a dedicated conformance
   target while remaining clear that C23 is not yet a claimed full-language
   target.
3. Convert more existing standards-relevant regression cases into explicit
   `-std=c99`, `-std=c11`, or selected `-std=c23` conformance entries where
   appropriate.
4. Add another external conformance layer beyond `tests/torture`, using the
   generic `test-conformance-external` hook so the next suite lands as data
   rather than more harness-specific plumbing.
5. Keep widening the explicit release gate itself, including future function-
   size and timing thresholds once those thresholds are formalized.
6. Prioritize remaining work by standards impact and coverage value, not by
   incidental source layout or single-program code-size wins.

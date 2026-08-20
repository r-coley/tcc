# Supported C Surface

This compiler now claims a freestanding ISO C99/C11 production target, with
validated `-std=c17` behavior and a substantial, regression-tested
`-std=c23` subset.

It does not claim hosted-library conformance, full ISO C89 coverage, or full
ISO C23 language conformance. Accepting a dialect flag such as `-std=c23`
still does not imply full language or library coverage for that standard.

The production conformance roadmap is tracked in
`docs/ISO_C_PRODUCTION_PLAN.md`. C99/C11 are the first claimed production
language targets; selected `-std=c23` behavior is measured explicitly but is
still tracked as a subset rather than a full claimed language target.

## Supported

- Preprocessing with object-like and function-like macros, conditional
  inclusion, token pasting, stringification, `_Pragma`, `#warning`, and
  macro-expanded `#include`
- Integer, pointer, array, struct, union, enum, `_Bool`, `void`, and
  function-pointer-heavy programs across the currently validated C99/C11
  conformance surface
- Typedefs, nested declarators, old-style and prototype function declarations
  in the currently validated compatibility cases
- Local, global, static-local, and extern declarations across the currently
  tested object and function-pointer forms
- Bit-fields and VLAs in the currently implemented and regression-tested cases,
  including runtime `sizeof`/cleanup semantics and block-scope pointer-to-VLA
  local initialization/use in the validated subset
- Installed-compiler use outside the source tree with non-boot standard-header
  and Darwin system-header smoke coverage
- Multi-target assembly generation, with the strongest current execution
  validation on arm64 and x64

## Partially Supported

- `float` and `double` scalar layout, literals, arithmetic, comparisons, casts,
  `float`/`double` conversions, fixed calls/returns, and promoted variadic tail
  arguments in the currently tested arm64 subset; struct-return calls with
  floating scalar parameters are also covered in that subset
- Type compatibility and qualifier semantics
- Pointer and prototype compatibility diagnostics
- Aggregate initialization edge cases
- `#pragma` handling beyond the currently implemented forms
- Predefined macro coverage beyond the current implementation set
- Non-arm64/x64 backend execution coverage
- Broader external conformance inputs beyond the current vendored torture and
  in-tree regression/conformance manifests

## Not Supported Or Not Claimed

- Full hosted ISO C library support
- Full cross-target direct by-value `_Complex` function / ABI semantics.
  `_Imaginary` now has parser/type-query support for object and typedef
  declarations, including reordered spellings, plus `sizeof` / `_Alignof`
  coverage and plain cast/assignment/compound-literal flows. Focused
  `_Imaginary` float/double/long-double cast coverage is now in place, as are
  focused `_Imaginary float` to `_Imaginary double` assignment/conversion
  coverage, implicit `_Imaginary`-to-`_Complex`, `_Imaginary`-to-real,
  `_Complex`-to-`_Imaginary`, and real-scalar-to-`_Complex` assignment cases
  in the current covered float/double forms, plus direct `_Imaginary long double`
  local declaration-initializer conversion to `long double` and file-scope
  `_Imaginary long double` object plus file-scope `_Imaginary long double`
  to `long double` initialization plus file-scope `_Complex long double`
  initialization from `_Imaginary long double`, and same-type unary `+`/`-`,
  binary `+`/`-`, `+=`, plus the
  covered `imag*imag`, `imag/imag`, `imag*real`, `real*imag`, `imag/real`,
  and `real/imag` cases now work for the covered float/double forms. Mixed-
  width `_Imaginary float` / `_Imaginary double` additive promotion is also
  covered in the current regression surface, along with the exercised mixed
  `_Imaginary long double` additive path plus the current exercised
  `_Imaginary`/`_Imaginary` division-to-real, assignment-to-`_Complex long double`,
  and direct/function-pointer `_Imaginary`-parameter to `_Complex`
  return-coercion cases. Mixed additive `real +/- imag` and `imag - real` now yield `_Complex`
  results in the covered cases as ordinary expressions, and the resulting
  `_Complex` values are covered for chained additive and multiplicative use
  with real scalars in the current regression surface, including the exercised
  mixed `_Complex float` / `_Imaginary long double` promotion cases. Focused
  `_Complex long double` coverage now also includes mixed-width `_Complex double`
  to `_Complex long double` assignment/promotion, real and imaginary scalar
  compound assignment, imaginary-scalar division and reciprocal division in the
  current covered cases, long-double `_Complex` plus `_Imaginary`
  equality/inequality in the exercised case, mixed `_Complex`/real and
  `_Complex`/`_Imaginary` conditional-expression result typing in the current
  exercised long-double cases, runtime conditional selection and assignment in
  the current covered long-double cases, and by-value direct,
  function-pointer, and nested-call round trips on arm64. Direct-spelled
  `_Imaginary` prototypes, function definitions, direct calls, and
  function-pointer calls are now covered in the current regression surface,
  and mixed `_Complex`+`_Imaginary` equality/inequality comparisons are
  covered in the current float/double cases. Broader non-exercised
  promotions/conversions are not yet claimed.
  The currently claimed arm64 surface includes regression-tested by-value `_Complex float` /
  `_Complex double` call-and-return behavior for both same-translation-unit
  and external-call cases, while pointer-to-`_Complex` function signatures are
  covered by the current parser/declaration surface.
- Full ISO preprocessing edge-case coverage
- Full floating-point semantics, aggregate/HFA ABI coverage, and cross-target
  code generation
- Full ISO qualifier, promotion, and conversion semantics
- Full C23 language coverage
- Full ISO conformance claims outside the current freestanding C99/C11 target

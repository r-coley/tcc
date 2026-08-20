# Conformance Baseline

This document records the current verified standards and release-gate baseline
for the compiler.

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
make test-sqlite-smoke: pass
make test-release-gates: pass
sqlite smoke on Darwin arm64: pass
```

Interpretation:

- Freestanding C99/C11 is the first claimed production language target.
- C17 is measured and clean as a compatibility superset of the claimed
  freestanding target.
- C23 is measured explicitly and currently clean against the in-tree conformance
  manifest, but it is not yet claimed as a full production language target.
- Installed-compiler behavior is validated outside the source tree.
- SQLite remains the primary large-input integration smoke test.
- Sanitizer-backed validation is included in `make test-release-gates-core`.
- The external-suite harness is now generic via `make test-conformance-external`;
  `tests/torture` is the first populated vendored suite, and a second suite can
  now be added without more harness changes.

Out of scope for this baseline:

- Hosted libc conformance claims
- Full ISO C23 language conformance claims
- Broader external-suite pass claims beyond the current vendored torture subset
- Cross-target execution parity beyond the current arm64/x64 emphasis

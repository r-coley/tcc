# Conformance Baseline

This document records the current verified standards and release-gate baseline
for the compiler.

Current verified baseline:

```text
make stage2: pass
make test: pass, stage0 10073/10073 and stage1 10073/10073
make test-conformance-c99: pass, 2296/2296
make test-conformance-c11: pass, 2308/2308
make test-conformance-c17: pass, 2308/2308
make test-conformance-c23: pass, 2309/2309
make test-conformance-external-torture: pass, 220/220
make test-conformance-external-ctestsuite-scc: pass, 43/43
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
- The external-suite harness is generic via `make test-conformance-external`.
  Its current vendored baselines are `tests/torture` and the ISC-licensed,
  revision-pinned `tests/external/c-testsuite-scc` import.

Out of scope for this baseline:

- Hosted libc conformance claims
- Full ISO C23 language conformance claims
- Broader external-suite pass claims beyond the current vendored torture and
  c-testsuite/SCC subsets
- Cross-target execution parity beyond the current arm64/x64 emphasis

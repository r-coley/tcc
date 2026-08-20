# Core type semantic audit tests

This directory contains runtime tests for the future core type-system correctness
track.  They are intentionally **not** listed in `tests/manifest.txt` yet because
some of the semantics they check are not implemented by the current compiler.

Use these as enablement tests when adding real semantics for `long`,
`unsigned long`, `long long`, `void *`, pointer depth, and typedef-preserved
integer types.  Move each test into `tests/manifest.txt` only when the matching
compiler feature is implemented and stage0/stage1 bootstrap has been validated.

The tests assume the current self-host target is arm64/LP64, where:

```text
sizeof(int)        == 4
sizeof(long)       == 8
sizeof(long long)  == 8
sizeof(void *)     == 8
```

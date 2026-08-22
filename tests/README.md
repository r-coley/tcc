# Test layout

Tests are grouped by broad feature area and listed in `manifest.txt`.

Manifest entries use:

```text
run:relative/path.c:flags:expected_exit_code
```

`make test` reads the manifest, compiles each test with the ARM64 backend,
assembles/links it with `clang`, runs it, and compares the exit code.

For `run` and `todo` entries, `stdout=relative/path.expected` is an optional
flag that also compares exact program stdout. The expected file path is
relative to `tests/`. Existing entries without this flag remain exit-code-only.

External or vendored suites can also be driven through the same harness:

```text
make test-conformance-external EXTERNAL_SUITE_CATEGORY=torture
make test-conformance-external EXTERNAL_SUITE_MANIFEST=tests/external/<suite>.manifest.txt
```

Use category mode when the suite already lives under a dedicated top-level
test directory present in `manifest.txt`. Use manifest mode when the suite
ships with its own case list or should remain isolated from the main manifest.

# Test layout

Tests are grouped by broad feature area and listed in `manifest.txt`.

Manifest entries use:

```text
relative/path.c:expected_exit_code
```

`make test` reads the manifest, compiles each test with the ARM64 backend,
assembles/links it with `clang`, runs it, and compares the exit code.

# External Conformance Suites

This directory is reserved for vendored or imported external conformance
inputs that should be runnable through the normal `tests/:runtests.sh`
harness.

The intended model is:

- keep the imported source files under a suite-specific subdirectory
- add a manifest file that lists only that suite's runnable cases
- invoke the suite through the generic external hook in the top-level
  `Makefile`

Examples:

```text
make test-conformance-external EXTERNAL_SUITE_CATEGORY=torture
make test-conformance-external EXTERNAL_SUITE_MANIFEST=tests/external/<suite>.manifest.txt
```

Notes:

- `tests/torture` remains the current vendored external baseline.
- `tests/external/c-testsuite-scc` is a second, revision-pinned external
  baseline. Run it with `make test-conformance-external-ctestsuite-scc`.
- New suites should prefer manifest mode over category filtering when their
  layout or expectations differ from the main in-tree manifest.
- External suites should be recorded in `docs/CONFORMANCE_BASELINE.md` once
  they have a stable pass/fail baseline.

# Build, Flags, And Performance Workflow

This note documents the bootstrap stage naming, the most useful compiler and
Makefile flags, and the current optimization plan.

## Stage Naming

- `build/tcc_stage0`
  Host-built compiler. Built by the system C compiler. This is the bootstrap
  compiler used to produce stage1.
- `build/tcc_stage1`
  Self-hosted compiler. Built by stage0. This is the compiler that should be
  treated as the real product and the compiler that `make install` installs.
- `build/tcc_stage2`
  Stability-check compiler. Built by stage1. Used to verify bootstrap
  consistency.
- `build/tcc`
  Compatibility symlink to `build/tcc_stage0`. Kept so older scripts do not
  break immediately.

The practical rule is:

- correctness must stay green for stage0, stage1, and stage2
- performance work is judged primarily on `build/tcc_stage1`
- stage0 is a diagnostic baseline, not the finish line

## Common Make Targets

```sh
make stage0
make stage1
make stage2
make test-native
make test-full
make install
```

What they do:

- `make stage0`
  Builds `build/tcc_stage0` and runs the native stage0 regression suite.
- `make stage1`
  Builds `build/tcc_stage1` with stage0 and runs the native stage1 suite.
- `make stage2`
  Builds `build/tcc_stage2` with stage1 and runs the bootstrap stress checks.
- `make test-native`
  Runs the normal host-target workflow: stage0 tests, stage1 tests, audit, and
  debug-section smoke.
- `make test-full`
  Runs the broader validation pass, including stress and optional backend smoke
  or runtime coverage.
- `make install`
  Installs `build/tcc_stage1` to `/usr/local/bin/tcc`.

Useful Make variables:

```sh
make TEST_TARGET=arm64
make TEST_AS=clang
make BOOT_FLAGS=-boot
make SELFHOST_CFLAGS=-g
```

The important current defaults from the Makefile are:

```text
BOOT_FLAGS      = -boot
SELFHOST_CFLAGS = -g
TEST_CFLAGS     = $(BOOT_FLAGS)
```

## Useful Compiler Flags

These come directly from `cc/main.c`.

### Build / output control

```text
-c                  Compile to object file via system assembler
-S                  Emit assembly
-E                  Preprocess only
-o <file>           Select output path
-###                Print compile / assemble / link phases
```

### Target selection

```text
-target=x86|x64|x86_64|arm64|mips|m68k
-mcpu=68000|68010|68020|68030|68040
-asm=nasm|gas|att
```

### Debug / diagnostics

```text
-g
-gdwarf-5
-dump-ast
-dump-ir
-dump-ir-lowered
-dump-cfg
-ftrace-codegen
-ftime-report
```

### Frontend / include control

```text
-boot
--bootstrap-includes
-nostdinc
-isystem <dir>
-std=c89|c90|c99|c11|c17|c23
```

### IR / fallback control

```text
-fno-ast-fallback
-fir-struct-fallback-only
-fir-strict
```

## Timing And Profiling Recipes

### Per-file compiler timing

Use `-ftime-report` when you want phase timings from the compiler itself.

```sh
./build/tcc_stage0 -ftime-report -c path/to/file.c -o /tmp/file.o
./build/tcc_stage1 -ftime-report -c path/to/file.c -o /tmp/file.o
```

For a two-file sqlite-style link:

```sh
./build/tcc_stage1 -ftime-report main.c sqlite3.c -lpthread -o foo
```

### Phase trace

Use `-###` to see the compile, assemble, and link phases as they execute.

```sh
./build/tcc_stage1 -### main.c sqlite3.c -lpthread -o foo
```

### Codegen-path trace

Use `-ftrace-codegen` when diagnosing whether a source path is going through
IR lowering or falling back to the AST emitter.

```sh
./build/tcc_stage1 -boot -ftrace-codegen -S cc/parser.c -o /tmp/parser.s
```

### Keep lowered IR output

`-dump-ir`, `-dump-cfg`, and `-dump-ast` now stop after emitting the dump, so
you can keep the output directly with `-o` instead of racing the temporary file
cleanup.

```sh
./build/tcc_stage1 -dump-ir -o /tmp/sqlite3.ir sqlite3.c
./build/tcc_stage1 -dump-cfg -o /tmp/sqlite3.cfg sqlite3.c
```

### Compare stage0 vs stage1 assembly quality

Use the existing helper:

```sh
make compare-asm
```

Or call it directly:

```sh
TEST_TARGET=arm64 BOOT_FLAGS=-boot tests/asm_compare.sh
```

### Compare function-by-function binary code size

For Mach-O binaries on macOS, use the repo-local Python helpers under
`tools/`.

Build a reference clang binary and a tcc binary first:

```sh
/usr/bin/clang main.c sqlite3.c -lpthread -o /tmp/sqlite_clang_foo
~/Projects/Programming/tcc/build/tcc_stage1 main.c sqlite3.c -lpthread -o /tmp/sqlite_tcc_foo
```

Count instructions per function and compare two binaries:

```sh
python3 tools/compare_func_sizes.py /tmp/sqlite_clang_foo /tmp/sqlite_tcc_foo
python3 tools/compare_func_sizes.py --sort delta --descending --only-different --limit 20 /tmp/sqlite_clang_foo /tmp/sqlite_tcc_foo
```

Or use the repo-level helpers that compare the current bootstrap stages
directly:

```sh
make report-selfhost-func-sizes
make report-sqlite-func-sizes
```

The default output is three columns:

```text
function  clang  tcc
```

and the default sort order is ascending clang size, which is useful for the
"smallest functions first" workflow.

Extract one named function from a binary:

```sh
python3 tools/extract_func_asm.py /tmp/sqlite_tcc_foo _sqlite3WhereUsesDeferredSeek
python3 tools/extract_func_asm.py --count-only /tmp/sqlite_tcc_foo _sqlite3WhereUsesDeferredSeek
```

### sqlite3 benchmark workflow

Host compiler:

```sh
cd ~/Projects/Programming/c/sqlite
time ./MK
```

Installed self-hosted compiler:

```sh
cd ~/Projects/Programming/c/sqlite
time ./MK2
```

Direct stage comparisons:

```sh
cd ~/Projects/Programming/c/sqlite
/usr/bin/time -p ~/Projects/Programming/tcc/build/tcc_stage0 main.c sqlite3.c -lpthread -o /tmp/sqlite_stage0
/usr/bin/time -p ~/Projects/Programming/tcc/build/tcc_stage1 main.c sqlite3.c -lpthread -o /tmp/sqlite_stage1
```

Or use the repo-level timing helper:

```sh
make report-sqlite-stage-times
make report-release-metrics
```

## Current Performance Snapshot

As measured on July 3, 2026 on this repo state:

```text
gcc via ./MK:                         about 0.74s real
stage0 (build/tcc_stage0):           about 3.03s real
stage1 (build/tcc_stage1 / MK2):     about 5.3s to 5.7s real
```

For the full sqlite build under `-ftime-report`, the broad shape is now:

```text
preprocess: about 0.50s
parse:      about 0.62s
ast+ir+emit: about 0.21s combined
assemble:   about 1.36s
link:       about 0.04s
total:      about 2.78s
```

The important conclusion is that preprocess is no longer the dominant problem.
The biggest remaining bucket is the assembly path, with parsing next.

## Next Optimization Plan

### Priority 1: Stage1 code quality against stage0 baseline

Goal:

- find where `build/tcc_stage1` emits worse code for the compiler sources than
  `build/tcc_stage0`

Method:

```sh
./build/tcc_stage0 -boot -S cc/parser.c -o /tmp/parser.stage0.s
./build/tcc_stage1 -boot -S cc/parser.c -o /tmp/parser.stage1.s
./build/tcc_stage0 -boot -S cc/preprocess.c -o /tmp/preprocess.stage0.s
./build/tcc_stage1 -boot -S cc/preprocess.c -o /tmp/preprocess.stage1.s
```

Start with hot self-hosted files:

- `cc/parser.c`
- `cc/preprocess.c`
- `cc/emit.c`
- `cc/codegen/arm64.c`

Look for:

- excess spills and reloads
- redundant loads of globals or locals
- unnecessary stack traffic
- repeated address recomputation
- bloated call sequences

### Priority 2: Assembly-path cost

Goal:

- reduce the largest remaining timing bucket

Areas to inspect:

- emitted assembly volume from TCC
- avoidable work in system assembler invocation flow
- repeated or overly verbose emission patterns
- obvious instruction-count inflation in hot generated functions

Useful spot checks:

```sh
wc -l /tmp/parser.stage0.s /tmp/parser.stage1.s
wc -l /tmp/preprocess.stage0.s /tmp/preprocess.stage1.s
```

### Priority 3: Parser hot paths

Goal:

- keep trimming stage1 frontend time after the earlier major wins

Focus areas:

- remaining identifier lookup churn
- declarator-heavy paths
- function-parameter handling
- field lookup and type reconstruction cases that still miss caches

This is now a secondary target behind assembly-path and stage1 code quality.

### Priority 4: Revisit preprocess only after the above

Goal:

- avoid spending time on a bucket that is no longer first-order

Preprocess should only move back up the queue if a fresh measurement shows it
regressing or if an external sampler points at a specific hot function there.

## Working Rules For Optimization

- optimize with `build/tcc_stage1` as the primary success metric
- keep `build/tcc_stage0` as the comparison baseline
- measure before and after every nontrivial change
- prefer small changes with a timing proof over broad speculative rewrites
- keep `make stage2` and `make test-native` green while iterating

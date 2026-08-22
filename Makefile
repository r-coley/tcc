CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
DEPFLAGS = -MMD -MP
SELFHOST_CFLAGS ?= -g
BOOT_FLAGS ?= -boot
TEST_CFLAGS ?= $(BOOT_FLAGS)
TEST_VERBOSE ?= $(V)
SANITIZE_FLAGS ?= -fsanitize=address,undefined -fno-omit-frame-pointer
SANITIZE_CFLAGS ?= $(CFLAGS) $(SANITIZE_FLAGS)
SANITIZE_LDFLAGS ?= $(SANITIZE_FLAGS)

SRC_DIR = cc
TEST_DIR = tests
BUILD_DIR = build
TMP_DIR = $(BUILD_DIR)/tmp
TMP_DIR_RELEASE_CORE = $(BUILD_DIR)/tmp-release-core
TMP_DIR_RELEASE_INSTALLED = $(BUILD_DIR)/tmp-release-installed
TMP_DIR_CONFORMANCE_C99 = $(TMP_DIR)/conformance-c99
TMP_DIR_CONFORMANCE_C11 = $(TMP_DIR)/conformance-c11
TMP_DIR_CONFORMANCE_C17 = $(TMP_DIR)/conformance-c17
TMP_DIR_CONFORMANCE_C23 = $(TMP_DIR)/conformance-c23
EXTERNAL_SUITE_CATEGORY ?= torture
EXTERNAL_SUITE_MANIFEST ?=
TMP_DIR_CONFORMANCE_EXTERNAL = $(TMP_DIR)/conformance-external
CONFORMANCE_EXTERNAL_CTESTSUITE_SCC_MANIFEST = $(TEST_DIR)/external/c-testsuite-scc.manifest.txt

# ---------------------------------------------------------------------------
# Stage0: the compiler built by the host toolchain (gcc)
# Stage1: the compiler built by stage0 targeting the native arch
# ---------------------------------------------------------------------------

STAGE0  = $(BUILD_DIR)/tcc_stage0
STAGE1  = $(BUILD_DIR)/tcc_stage1
LEGACY_STAGE0 = $(BUILD_DIR)/tcc
SANITIZE_STAGE0 = $(BUILD_DIR)/tcc_asan
SANITIZE_DIR = $(BUILD_DIR)/asan

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/lexer.c \
	$(SRC_DIR)/preprocess.c \
	$(SRC_DIR)/parser.c \
	$(SRC_DIR)/expr.c \
	$(SRC_DIR)/stmt.c \
	$(SRC_DIR)/ast.c \
	$(SRC_DIR)/ir.c \
	$(SRC_DIR)/emit.c \
	$(SRC_DIR)/data_emit.c \
	$(SRC_DIR)/debug_info.c \
	$(SRC_DIR)/helpers.c \
	$(SRC_DIR)/codegen/x86.c \
	$(SRC_DIR)/codegen/x64.c \
	$(SRC_DIR)/codegen/arm64.c \
	$(SRC_DIR)/codegen/arm64_peephole.c \
	$(SRC_DIR)/codegen/mips.c \
	$(SRC_DIR)/codegen/m68k.c

STAGE0_OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
STAGE0_DEPS = $(STAGE0_OBJS:.o=.d)
SANITIZE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(SANITIZE_DIR)/%.o,$(SRCS))
SANITIZE_DEPS = $(SANITIZE_OBJS:.o=.d)
HDRS = $(shell find $(SRC_DIR) -name '*.h' -print)

# Auto-detect target from host arch; override with TEST_TARGET=arm64 etc.
HOST_OS   := $(shell uname -s 2>/dev/null || echo unknown)
HOST_ARCH := $(shell uname -m 2>/dev/null || echo unknown)
ifeq ($(HOST_ARCH),arm64)
TEST_TARGET ?= arm64
else ifeq ($(HOST_ARCH),aarch64)
TEST_TARGET ?= arm64
else ifeq ($(HOST_ARCH),x86_64)
TEST_TARGET ?= x64
else ifeq ($(HOST_ARCH),amd64)
TEST_TARGET ?= x64
else ifeq ($(HOST_ARCH),i386)
TEST_TARGET ?= x86
else ifeq ($(HOST_ARCH),i486)
TEST_TARGET ?= x86
else ifeq ($(HOST_ARCH),i586)
TEST_TARGET ?= x86
else ifeq ($(HOST_ARCH),i686)
TEST_TARGET ?= x86
else
$(error Unsupported host architecture '$(HOST_ARCH)'; set TEST_TARGET manually)
endif

# Assembler used to link test binaries — override with TEST_AS=gcc etc.
TEST_AS ?= clang

# Tool used by test-x64-run to assemble/link generated x64 assembly.
# On Apple Silicon, default to x86_64 so clang assembles the generated
# Intel x64 assembly instead of trying to parse it as arm64.
X64_RUN_CC ?= clang
X64_RUN_EXTRA_LDFLAGS ?=
X64_RUN_DEFAULT_LDFLAGS :=

ifeq ($(HOST_OS),Darwin)
ifeq ($(HOST_ARCH),arm64)
X64_RUN_DEFAULT_LDFLAGS := -arch x86_64
endif
endif

X64_RUN_LDFLAGS ?= $(X64_RUN_DEFAULT_LDFLAGS) $(X64_RUN_EXTRA_LDFLAGS)

# Tool used by test-x86-run to assemble/link generated 32-bit x86 assembly.
# This is deliberately host-optional: many modern hosts, especially Apple
# Silicon macOS, cannot link or execute 32-bit x86 binaries.
X86_RUN_CC ?= clang
X86_RUN_CFLAGS ?= -m32
X86_RUN_EXTRA_LDFLAGS ?=
X86_RUN_LDFLAGS ?= $(X86_RUN_CFLAGS) $(X86_RUN_EXTRA_LDFLAGS)
X86_RUN_VERBOSE_SKIP ?= 0

# Tooling for optional MIPS execution smoke coverage. This remains host-optional:
# most development hosts do not have a MIPS cross compiler or qemu-user installed.
MIPS_RUN_CC ?= mips-linux-gnu-gcc
MIPS_RUN_QEMU ?= qemu-mips
MIPS_RUN_CFLAGS ?= -static
MIPS_RUN_EXTRA_LDFLAGS ?=
MIPS_RUN_LDFLAGS ?= $(MIPS_RUN_CFLAGS) $(MIPS_RUN_EXTRA_LDFLAGS)
MIPS_RUN_QEMU_ARGS ?=
MIPS_RUN_VERBOSE_SKIP ?= 0

# Tooling for optional m68k execution smoke coverage. Like MIPS, this remains
# host-optional because most development hosts do not ship an m68k cross
# compiler or qemu-user emulator by default.
M68K_RUN_CC ?= m68k-linux-gnu-gcc
M68K_RUN_QEMU ?= qemu-m68k
M68K_RUN_CFLAGS ?= -static
M68K_RUN_EXTRA_LDFLAGS ?=
M68K_RUN_LDFLAGS ?= $(M68K_RUN_CFLAGS) $(M68K_RUN_EXTRA_LDFLAGS)
M68K_RUN_QEMU_ARGS ?=
M68K_RUN_VERBOSE_SKIP ?= 0

ifneq ($(filter 1 y Y yes YES true TRUE on ON,$(TEST_VERBOSE)),)
RUNTESTS_VERBOSE_FLAG = -v
else
RUNTESTS_VERBOSE_FLAG =
endif

# Test runner — single unified runner for all test types
RUNTESTS0 = TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
	-c $(STAGE0) \
	-t $(TEST_TARGET) \
	-a $(TEST_AS) \
	-m $(TEST_DIR)/manifest.txt \
	$(RUNTESTS_VERBOSE_FLAG) \
	-D $(TMP_DIR)

RUNTESTS1 = TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
	-c $(STAGE1) \
	-t $(TEST_TARGET) \
	-a $(TEST_AS) \
	-m $(TEST_DIR)/manifest.txt \
	$(RUNTESTS_VERBOSE_FLAG) \
	-D $(TMP_DIR)

RUNTESTS_X86_RUNTIME = TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
	-c $(STAGE0) \
	-t x86 \
	-a $(TEST_AS) \
	-m $(TEST_DIR)/manifest-x86.txt \
	$(RUNTESTS_VERBOSE_FLAG) \
	-D $(TMP_DIR)

RUNTESTS_SANITIZE = TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
	-c $(SANITIZE_STAGE0) \
	-t $(TEST_TARGET) \
	-a $(TEST_AS) \
	-m $(TEST_DIR)/manifest.txt \
	$(RUNTESTS_VERBOSE_FLAG) \
	-D $(TMP_DIR)/asan

TEST_CATEGORIES = \
	abi \
	arrays \
	chars \
	control \
	core \
	globals \
	loops \
	lvalue \
	opt \
	pointers \
	preproc \
	strings \
	structs \
	types

STAGE0_ALIAS_TARGETS = \
	test-errors \
	test-stage0-success \
	test-stage0-errors \
	test-stage0-debug-lines \
	test-flags \
	test-debug-lines

STAGE1_ALIAS_TARGETS = \
	test-stage1-success \
	test-stage1-errors \
	test-stage1-debug-lines

BACKEND_SMOKE_TARGETS = \
	test-x64-smoke \
	test-x86-smoke \
	test-mips-smoke \
	test-m68k-smoke \
	test-debug-sections

BACKEND_RUN_TARGETS = \
	test-x64-run \
	test-x86-run \
	test-mips-run \
	test-m68k-run

CONFORMANCE_C99_MANIFEST = $(TMP_DIR)/manifest-conformance-c99.txt
CONFORMANCE_C17_MANIFEST = $(TMP_DIR)/manifest-conformance-c17.txt
CONFORMANCE_C11_MANIFEST = $(TMP_DIR)/manifest-conformance-c11.txt
CONFORMANCE_C23_MANIFEST = $(TMP_DIR)/manifest-conformance-c23.txt

.PHONY: test-m68k-smoke all stage0 stage1 stage2 clean clobber test-stress test-nonboot-stdbool test-nonboot-stddef test-nonboot-c11-headers test-nonboot-stdint test-nonboot-stdatomic test-nonboot-system-headers test-installed-smoke compare-asm \
	test test-native test-full test-stage0 test-stage0-success test-stage1 test-stage1-success test-asan test-sanitize audit audit-core-types smoketest install uninstall \
	dump-ast test-cfg arm64 x86 test-x86 test-x86-runtime test-x86-smoke test-x86-aggregate-smoke test-x86-run test-x64 test-x64-smoke test-x64-run test-x64-complex-run test-mips test-mips-smoke test-mips-run test-m68k-run test-backend-smoke test-backend-run test-debug-sections mips \
	test-conformance-c99 test-conformance-c11 test-conformance-c17 test-conformance-c23 test-conformance-external test-conformance-external-torture test-conformance-external-ctestsuite-scc test-sqlite-smoke test-release-gates-core test-release-gates-installed test-release-gates \
	report-selfhost-func-sizes report-sqlite-func-sizes report-sqlite-stage-times report-release-metrics \
	$(addprefix test-,$(TEST_CATEGORIES)) \
	$(addprefix test-stage1-,$(TEST_CATEGORIES)) \
	$(STAGE0_ALIAS_TARGETS) \
	$(STAGE1_ALIAS_TARGETS) \
	test-ir-strict test-preprocess test-lldb-smoke test-stage0-lldb-smoke test-stage1-lldb-smoke

# Default: build the host-built bootstrap compiler (stage0)
all: stage0

# ---------------------------------------------------------------------------
# Stage0: built by host gcc
# ---------------------------------------------------------------------------

stage0: $(STAGE0) $(LEGACY_STAGE0) test-stage0

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TMP_DIR): | $(BUILD_DIR)
	@mkdir -p $(TMP_DIR)

$(TMP_DIR_CONFORMANCE_C99) $(TMP_DIR_CONFORMANCE_C11) \
$(TMP_DIR_CONFORMANCE_C17) $(TMP_DIR_CONFORMANCE_C23) \
$(TMP_DIR_CONFORMANCE_EXTERNAL): | $(TMP_DIR)
	@mkdir -p $@

$(STAGE0): $(STAGE0_OBJS)
	$(CC) $(CFLAGS) -o $(STAGE0) $(STAGE0_OBJS)

$(LEGACY_STAGE0): $(STAGE0) | $(BUILD_DIR)
	ln -sf $(notdir $(STAGE0)) $(LEGACY_STAGE0)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -I$(SRC_DIR) -I$(SRC_DIR)/codegen -c $< -o $@

# ---------------------------------------------------------------------------
# Stage1: built by stage0 targeting the native arch
# ---------------------------------------------------------------------------

SELFHOST_TARGET ?= arm64
SELFHOST_DIR     = $(BUILD_DIR)/stage1

STAGE1_OBJS = $(patsubst $(SRC_DIR)/%.c,$(SELFHOST_DIR)/%.o,$(SRCS))
$(STAGE1_OBJS): $(HDRS)

stage1: $(STAGE1) test-stage1
	@echo

$(STAGE1): $(STAGE1_OBJS) | $(SELFHOST_DIR)
	clang -g $^ -o $@

$(SELFHOST_DIR)/%.o: $(SRC_DIR)/%.c $(STAGE0) | $(SELFHOST_DIR)
	@mkdir -p $(dir $@)
	$(STAGE0) $(BOOT_FLAGS) $(SELFHOST_CFLAGS) -c $< -o $@
#	$(STAGE0) $(BOOT_FLAGS) -target=$(SELFHOST_TARGET) $(SELFHOST_CFLAGS) -c $< -o $@

$(SELFHOST_DIR): | $(BUILD_DIR)
	@mkdir -p $@

# ---------------------------------------------------------------------------
# Stage2: built by stage1 (bootstrap stability check)
# ---------------------------------------------------------------------------

STAGE2     = $(BUILD_DIR)/tcc_stage2
STAGE2_DIR = $(BUILD_DIR)/stage2
STAGE2_OBJS = $(patsubst $(SRC_DIR)/%.c,$(STAGE2_DIR)/%.o,$(SRCS))
$(STAGE2_OBJS): $(HDRS)

# Header dependencies that affect the Codegen ABI/layout.
# Keep these explicit so changing the Codegen function table rebuilds every
# stage/backend object instead of linking stale objects with a new struct layout.
CODEGEN_HDR = $(SRC_DIR)/codegen/codegen.h
$(STAGE0_OBJS): $(CODEGEN_HDR)
$(STAGE1_OBJS): $(CODEGEN_HDR)
$(STAGE2_OBJS): $(CODEGEN_HDR)
$(SANITIZE_OBJS): $(CODEGEN_HDR)

$(STAGE2_DIR): | $(BUILD_DIR)
	@mkdir -p $@

$(STAGE2_DIR)/%.o: $(SRC_DIR)/%.c $(STAGE1) | $(STAGE2_DIR)
	@mkdir -p $(dir $@)
	$(STAGE1) $(BOOT_FLAGS) $(SELFHOST_CFLAGS) -c $< -o $@
#	$(STAGE1) $(BOOT_FLAGS) -target=$(SELFHOST_TARGET) $(SELFHOST_CFLAGS) -c $< -o $@

$(STAGE2): $(STAGE2_OBJS)
	clang -g $^ -o $@

stage2: $(STAGE2) test-stress

# ---------------------------------------------------------------------------
# Cleaning
# ---------------------------------------------------------------------------

clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		chmod -R u+w "$(BUILD_DIR)" 2>/dev/null || true; \
		find "$(BUILD_DIR)" -mindepth 1 -depth -exec rm -rf {} + 2>/dev/null || true; \
		rmdir "$(BUILD_DIR)" 2>/dev/null || rm -rf "$(BUILD_DIR)" 2>/dev/null || true; \
	fi
	rm -f out out.s out.o *.asm *.s *.o *.s.opt test-results.txt test.o test.s

clobber: clean
	rm -rf *.dSYM

# ---------------------------------------------------------------------------
# Test targets — stage0
# ---------------------------------------------------------------------------

# 'make test' is the normal native-host development workflow.
# It keeps the default run focused on the host target selected by TEST_TARGET
# (arm64 on Apple Silicon, x64 on x86_64 hosts) and skips the broader
# cross-target backend/stress passes.
test: test-native

test-native: test-stage0 test-stage1 audit test-debug-sections

# 'make test-full' preserves the broader validation pass: native suites plus
# stage2/bootstrap stress checks and optional cross-target/backend smoke/run
# targets for final confidence runs.
test-full: test-native test-stress test-backend-smoke test-backend-run

compare-asm: $(STAGE1) | $(TMP_DIR)
	TEST_TARGET=$(TEST_TARGET) BOOT_FLAGS="$(BOOT_FLAGS)" $(TEST_DIR)/asm_compare.sh

$(CONFORMANCE_C99_MANIFEST): $(TEST_DIR)/manifest.txt | $(TMP_DIR)
	@grep -E '^(run|error|warn|nowarn|dwarf|dwarfverify|todo):' $< | grep -- '-std=c99' > $@

$(CONFORMANCE_C17_MANIFEST): $(TEST_DIR)/manifest.txt | $(TMP_DIR)
	@grep -E '^(run|error|warn|nowarn|dwarf|dwarfverify|todo):' $< | grep -- '-std=c17' > $@

$(CONFORMANCE_C11_MANIFEST): $(TEST_DIR)/manifest.txt | $(TMP_DIR)
	@grep -E '^(run|error|warn|nowarn|dwarf|dwarfverify|todo):' $< | grep -- '-std=c11' > $@

$(CONFORMANCE_C23_MANIFEST): $(TEST_DIR)/manifest.txt | $(TMP_DIR)
	@grep -E '^(run|error|warn|nowarn|dwarf|dwarfverify|todo):' $< | grep -- '-std=c23' > $@

test-stage0: $(STAGE0) | $(TMP_DIR)
	$(RUNTESTS0)

test-conformance-c99: $(STAGE0) $(CONFORMANCE_C99_MANIFEST) | $(TMP_DIR_CONFORMANCE_C99)
	TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
		-c $(STAGE0) \
		-t $(TEST_TARGET) \
		-a $(TEST_AS) \
		-m $(CONFORMANCE_C99_MANIFEST) \
		$(RUNTESTS_VERBOSE_FLAG) \
		-D $(TMP_DIR_CONFORMANCE_C99)

test-conformance-c11: $(STAGE0) $(CONFORMANCE_C11_MANIFEST) | $(TMP_DIR_CONFORMANCE_C11)
	TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
		-c $(STAGE0) \
		-t $(TEST_TARGET) \
		-a $(TEST_AS) \
		-m $(CONFORMANCE_C11_MANIFEST) \
		$(RUNTESTS_VERBOSE_FLAG) \
		-D $(TMP_DIR_CONFORMANCE_C11)

test-conformance-c17: $(STAGE0) $(CONFORMANCE_C17_MANIFEST) | $(TMP_DIR_CONFORMANCE_C17)
	TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
		-c $(STAGE0) \
		-t $(TEST_TARGET) \
		-a $(TEST_AS) \
		-m $(CONFORMANCE_C17_MANIFEST) \
		$(RUNTESTS_VERBOSE_FLAG) \
		-D $(TMP_DIR_CONFORMANCE_C17)

test-conformance-c23: $(STAGE0) $(CONFORMANCE_C23_MANIFEST) | $(TMP_DIR_CONFORMANCE_C23)
	TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
		-c $(STAGE0) \
		-t $(TEST_TARGET) \
		-a $(TEST_AS) \
		-m $(CONFORMANCE_C23_MANIFEST) \
		$(RUNTESTS_VERBOSE_FLAG) \
		-D $(TMP_DIR_CONFORMANCE_C23)

test-conformance-external: $(STAGE0) | $(TMP_DIR_CONFORMANCE_EXTERNAL)
	@set -eu; \
	if [ -n "$(strip $(EXTERNAL_SUITE_MANIFEST))" ]; then \
		TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
			-c $(STAGE0) \
			-t $(TEST_TARGET) \
			-a $(TEST_AS) \
			-m "$(EXTERNAL_SUITE_MANIFEST)" \
			$(RUNTESTS_VERBOSE_FLAG) \
			-D $(TMP_DIR_CONFORMANCE_EXTERNAL); \
	else \
		TCC_TEST_FLAGS="$(TEST_CFLAGS)" $(TEST_DIR)/:runtests.sh \
			-c $(STAGE0) \
			-t $(TEST_TARGET) \
			-a $(TEST_AS) \
			-m $(TEST_DIR)/manifest.txt \
			$(RUNTESTS_VERBOSE_FLAG) \
			-D $(TMP_DIR_CONFORMANCE_EXTERNAL) \
			-C "$(EXTERNAL_SUITE_CATEGORY)"; \
	fi

test-conformance-external-torture: EXTERNAL_SUITE_CATEGORY = torture
test-conformance-external-torture: test-conformance-external

test-conformance-external-ctestsuite-scc: EXTERNAL_SUITE_MANIFEST = $(CONFORMANCE_EXTERNAL_CTESTSUITE_SCC_MANIFEST)
test-conformance-external-ctestsuite-scc: test-conformance-external

$(SANITIZE_DIR):
	@mkdir -p $@

$(SANITIZE_DIR)/%.o: $(SRC_DIR)/%.c | $(SANITIZE_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(SANITIZE_CFLAGS) $(DEPFLAGS) -I$(SRC_DIR) -I$(SRC_DIR)/codegen -c $< -o $@

$(SANITIZE_STAGE0): $(SANITIZE_OBJS)
	$(CC) $(SANITIZE_CFLAGS) $(SANITIZE_LDFLAGS) -o $@ $^

test-asan: $(SANITIZE_STAGE0) | $(TMP_DIR)
	$(RUNTESTS_SANITIZE)

test-sanitize: test-asan

-include $(STAGE0_DEPS) $(SANITIZE_DEPS)

# x86 currently has an assembly-generation gate. The runtime manifest is kept
# separate because modern macOS/ARM64 hosts cannot link or execute 32-bit x86
# binaries directly.
test-x86: test-x86-smoke
	@echo "NOTE: x86 runtime tests are available as make test-x86-runtime on a host/toolchain that can run 32-bit x86 binaries."

test-x86-runtime: $(STAGE0) | $(TMP_DIR)
	$(RUNTESTS_X86_RUNTIME)

define SIMPLE_ALIAS_RULE
$(1): $(2)
endef

$(foreach alias,$(STAGE0_ALIAS_TARGETS),$(eval $(call SIMPLE_ALIAS_RULE,$(alias),test-stage0)))

define STAGE0_CATEGORY_RULE
test-$(1): $(STAGE0) | $(TMP_DIR) ; $(RUNTESTS0) -C $(1)
endef

$(foreach category,$(TEST_CATEGORIES),$(eval $(call STAGE0_CATEGORY_RULE,$(category))))

# ---------------------------------------------------------------------------
# Test targets — stage1
# ---------------------------------------------------------------------------

# 'make test-stage1' also runs both the success suite and the
# diagnostic/error suite for the self-hosted compiler.
test-stage1: $(STAGE1) | $(TMP_DIR)
	$(RUNTESTS1)

$(foreach alias,$(STAGE1_ALIAS_TARGETS),$(eval $(call SIMPLE_ALIAS_RULE,$(alias),test-stage1)))

define STAGE1_CATEGORY_RULE
test-stage1-$(1): $(STAGE1) | $(TMP_DIR) ; $(RUNTESTS1) -C $(1)
endef

$(foreach category,$(TEST_CATEGORIES),$(eval $(call STAGE1_CATEGORY_RULE,$(category))))

# ---------------------------------------------------------------------------
# Specialised test modes
# ---------------------------------------------------------------------------

test-stage0-lldb-smoke: $(STAGE0) | $(TMP_DIR)
	sh $(TEST_DIR)/debug/:lldb_smoke.sh -c $(STAGE0) -t $(TEST_TARGET) -a $(TEST_AS) -T $(TMP_DIR)/lldb-smoke --flags "$(TEST_CFLAGS)"

test-stage1-lldb-smoke: $(STAGE1) | $(TMP_DIR)
	sh $(TEST_DIR)/debug/:lldb_smoke.sh -c $(STAGE1) -t $(TEST_TARGET) -a $(TEST_AS) -T $(TMP_DIR)/lldb-smoke-stage1 --flags "$(TEST_CFLAGS)"

# LLDB source-level smoke validation. This is intentionally opt-in because it
# requires LLDB and a runnable native target binary.
$(eval $(call SIMPLE_ALIAS_RULE,test-lldb-smoke,test-stage0-lldb-smoke))

test-preprocess: $(STAGE0) | $(TMP_DIR)
	@fail=0; \
	echo "Running preprocess tests:"; \
	for t in $(TEST_DIR)/preproc/*.expected; do \
		base=$${t%.expected}; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=x86 -E $$base.c > $(TMP_DIR)/pre.out 2>&1; then \
			echo "FAIL $$base (-E failed)"; \
			fail=$$((fail + 1)); \
			continue; \
		fi; \
		if diff -u $$t $(TMP_DIR)/pre.out > $(TMP_DIR)/pre.diff; then \
			printf "."; \
		else \
			echo; \
			echo "FAIL $$base (preprocess output mismatch)"; \
			cat $(TMP_DIR)/pre.diff; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	echo; \
	exit $$fail

test-ir-strict: $(STAGE0) | $(TMP_DIR)
	@fail=0; \
	tests=0; \
	echo "Running IR strict generation tests:"; \
	while IFS= read -r entry; do \
		case "$$entry" in ''|\#*) continue ;; esac; \
		kind=$${entry%%:*}; \
		case "$$kind" in run) ;; *) continue ;; esac; \
		rest=$${entry#*:}; \
		if [ "$$rest" = "$$entry" ]; then \
			continue; \
		fi; \
		t=$${rest%%:*}; \
		case "$$t" in *.c) ;; *) continue ;; esac; \
		rest=$${rest#*:}; \
		opts=$${rest%%:*}; \
		case "$$t" in *" "*) continue ;; esac; \
		case "$$opts" in *clangextra=*) continue ;; esac; \
		case "$$opts" in *extra=*) continue ;; esac; \
		base=$${t%.c}; \
		tests=$$((tests + 1)); \
		if $(STAGE0) -S -fir-strict -target=arm64 $$opts "$(TEST_DIR)/$$t" > $(TMP_DIR)/out.s 2>$(TMP_DIR)/ir.err; then \
			printf "."; \
		else \
			if ! grep -q "IR lowering/codegen incomplete" $(TMP_DIR)/ir.err; then \
				printf "x"; \
			elif grep -q "struct" $(TMP_DIR)/ir.err; then \
				printf "s"; \
			elif grep -qx "$$base" $(TEST_DIR)/ir_fallback_allowlist.txt; then \
				printf "a"; \
			else \
				echo; \
				echo "FAIL $$base (new non-struct IR fallback)"; \
				cat $(TMP_DIR)/ir.err; \
				fail=$$((fail + 1)); \
			fi; \
		fi; \
	done < $(TEST_DIR)/manifest.txt; \
	echo; \
	echo "------------------------------"; \
	echo "$$tests/$$tests Checked"; \
	echo "$$fail Failed"; \
	echo "------------------------------"; \
	exit $$fail

# ---------------------------------------------------------------------------
# Audit / future-semantics tests
# ---------------------------------------------------------------------------

# Informational audit target.  It runs tests for desired future core type
# semantics and reports unsupported cases as SKIP rather than failing the
# normal build.
audit: $(STAGE0) | $(TMP_DIR)
	$(RUNTESTS0) --type todo

# ---------------------------------------------------------------------------
# Stress / extended tests (make test-stress)
# These are correct by construction but slow — not part of the default suite.
# ---------------------------------------------------------------------------

STRESS_REF = $(SRC_DIR)/lexer.c

# Stage2 bootstrap equality: stage1 and stage2 must produce identical assembly
# for a reference file.  Identical output proves the compiler is stable.
test-stress: $(STAGE2) test-nonboot-stdbool test-nonboot-stddef test-nonboot-c11-headers test-nonboot-stdint test-nonboot-stdatomic test-nonboot-system-headers | $(TMP_DIR)
	@echo "Running stress / extended tests:"
	@echo ""
	@echo "--- Stage2 bootstrap equality ---"
	@$(STAGE1) $(BOOT_FLAGS) -target=$(SELFHOST_TARGET) -S $(STRESS_REF) -o $(TMP_DIR)/stress_stage1.s 2>/dev/null
	@$(STAGE2) $(BOOT_FLAGS) -target=$(SELFHOST_TARGET) -S $(STRESS_REF) -o $(TMP_DIR)/stress_stage2.s 2>/dev/null
	@if diff -q $(TMP_DIR)/stress_stage1.s $(TMP_DIR)/stress_stage2.s > /dev/null 2>&1; then \
		echo "  PASS stage1 == stage2 ($(STRESS_REF))"; \
	else \
		echo "  FAIL stage1 != stage2 on $(STRESS_REF)"; \
		diff $(TMP_DIR)/stress_stage1.s $(TMP_DIR)/stress_stage2.s | head -20; \
		exit 1; \
	fi
	@echo ""
	@echo "--- Cross-target assembly check ---"
	@for target in arm64 x64; do \
		if $(STAGE1) $(BOOT_FLAGS) -target=$$target -S $(STRESS_REF) -o $(TMP_DIR)/stress_$$target.s 2>/dev/null && \
		   [ -s $(TMP_DIR)/stress_$$target.s ]; then \
			echo "  PASS target=$$target"; \
		else \
			echo "  FAIL target=$$target (empty or error)"; \
			exit 1; \
		fi; \
	done
	@echo "  NOTE targets x86/mips skipped (limited self-hosting support)"
	@echo ""
	@echo "--- Preprocessor stress (-E on macro-heavy files) ---"
	@fail=0; \
	for src in $(TEST_DIR)/preproc/test_macro_long_body.c \
	           $(TEST_DIR)/preproc/test_macro_hash_many.c \
	           $(TEST_DIR)/torture/00200.c \
	           $(TEST_DIR)/torture/00204.c; do \
		if $(STAGE1) $(BOOT_FLAGS) -target=$(SELFHOST_TARGET) -E $$src > /dev/null 2>&1; then \
			echo "  PASS -E $$src"; \
		else \
			echo "  FAIL -E $$src"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	[ $$fail -eq 0 ] || exit 1
	@echo ""
	@echo "All stress tests passed."

test-nonboot-stdbool: $(STAGE1) | $(TMP_DIR)
	@set -u; \
	src_nohdr="$(TMP_DIR)/nonboot_stdbool_no_header.c"; \
	src_hdr="$(TMP_DIR)/nonboot_stdbool_with_header.c"; \
	exe_hdr="$(TMP_DIR)/nonboot_stdbool_with_header"; \
	err_nohdr="$(TMP_DIR)/nonboot_stdbool_no_header.err"; \
	err_hdr="$(TMP_DIR)/nonboot_stdbool_with_header.err"; \
	printf '%s\n' 'bool x = true; int main(void){ return x ? 42 : 1; }' > "$$src_nohdr"; \
	printf '%s\n%s\n' '#include <stdbool.h>' 'bool x = true; int main(void){ return x ? 42 : 1; }' > "$$src_hdr"; \
	echo "--- Non-boot stdbool smoke test ---"; \
	if $(STAGE1) -std=c11 "$$src_nohdr" -o "$(TMP_DIR)/nonboot_stdbool_no_header" >"$$err_nohdr" 2>&1; then \
		echo "  FAIL bool accepted without <stdbool.h> in C11"; \
		exit 1; \
	else \
		echo "  PASS bool requires <stdbool.h> in C11"; \
	fi; \
	if ! $(STAGE1) -std=c11 "$$src_hdr" -o "$$exe_hdr" >"$$err_hdr" 2>&1; then \
		echo "  FAIL <stdbool.h> C11 compile"; \
		sed 's/^/    /' "$$err_hdr"; \
		exit 1; \
	fi; \
	set +e; "$$exe_hdr" >/dev/null 2>>"$$err_hdr"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stdbool.h> C11 runtime"; \
	else \
		echo "  FAIL <stdbool.h> C11 runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo ""

test-nonboot-stddef: $(STAGE1) | $(TMP_DIR)
	@set -u; \
	src_nohdr="$(TMP_DIR)/nonboot_nullptr_no_header.c"; \
	src_hdr="$(TMP_DIR)/nonboot_nullptr_with_header.c"; \
	exe_hdr="$(TMP_DIR)/nonboot_nullptr_with_header"; \
	err_nohdr="$(TMP_DIR)/nonboot_nullptr_no_header.err"; \
	err_hdr="$(TMP_DIR)/nonboot_nullptr_with_header.err"; \
	printf '%s\n' 'nullptr_t p = nullptr; int main(void){ return p == 0 ? 42 : 1; }' > "$$src_nohdr"; \
	printf '%s\n%s\n' '#include <stddef.h>' 'nullptr_t p = nullptr; int main(void){ return p == 0 ? 42 : 1; }' > "$$src_hdr"; \
	echo "--- Non-boot stddef smoke test ---"; \
	if $(STAGE1) -std=c23 "$$src_nohdr" -o "$(TMP_DIR)/nonboot_nullptr_no_header" >"$$err_nohdr" 2>&1; then \
		echo "  FAIL nullptr_t accepted without <stddef.h> in C23"; \
		exit 1; \
	else \
		echo "  PASS nullptr_t requires <stddef.h> in C23"; \
	fi; \
	if ! $(STAGE1) -std=c23 "$$src_hdr" -o "$$exe_hdr" >"$$err_hdr" 2>&1; then \
		echo "  FAIL <stddef.h> C23 nullptr_t compile"; \
		sed 's/^/    /' "$$err_hdr"; \
		exit 1; \
	fi; \
	set +e; "$$exe_hdr" >/dev/null 2>>"$$err_hdr"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stddef.h> C23 nullptr_t runtime"; \
	else \
		echo "  FAIL <stddef.h> C23 nullptr_t runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo ""

test-nonboot-c11-headers: $(STAGE1) | $(TMP_DIR)
	@set -u; \
	src_align="$(TMP_DIR)/nonboot_stdalign_probe.c"; \
	exe_align="$(TMP_DIR)/nonboot_stdalign_probe"; \
	err_align="$(TMP_DIR)/nonboot_stdalign_probe.err"; \
	src_noreturn="$(TMP_DIR)/nonboot_stdnoreturn_probe.c"; \
	exe_noreturn="$(TMP_DIR)/nonboot_stdnoreturn_probe"; \
	err_noreturn="$(TMP_DIR)/nonboot_stdnoreturn_probe.err"; \
	printf '%s\n%s\n%s\n' '#include <stdalign.h>' 'alignas(16) int x;' 'int main(void){ return alignof(int) == 4 ? 42 : 1; }' > "$$src_align"; \
	printf '%s\n%s\n' '#include <stdnoreturn.h>' 'noreturn void die(void){ for(;;){} } int main(void){ return 42; }' > "$$src_noreturn"; \
	echo "--- Non-boot C11 header smoke test ---"; \
	if ! $(STAGE1) -std=c11 "$$src_align" -o "$$exe_align" >"$$err_align" 2>&1; then \
		echo "  FAIL <stdalign.h> C11 compile"; \
		sed 's/^/    /' "$$err_align"; \
		exit 1; \
	fi; \
	set +e; "$$exe_align" >/dev/null 2>>"$$err_align"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stdalign.h> C11 runtime"; \
	else \
		echo "  FAIL <stdalign.h> C11 runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	if ! $(STAGE1) -std=c11 "$$src_noreturn" -o "$$exe_noreturn" >"$$err_noreturn" 2>&1; then \
		echo "  FAIL <stdnoreturn.h> C11 compile"; \
		sed 's/^/    /' "$$err_noreturn"; \
		exit 1; \
	fi; \
	set +e; "$$exe_noreturn" >/dev/null 2>>"$$err_noreturn"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stdnoreturn.h> C11 runtime"; \
	else \
		echo "  FAIL <stdnoreturn.h> C11 runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo ""

test-nonboot-stdint: $(STAGE1) | $(TMP_DIR)
	@set -u; \
	src="$(TMP_DIR)/nonboot_stdint_probe.c"; \
	exe="$(TMP_DIR)/nonboot_stdint_probe"; \
	err="$(TMP_DIR)/nonboot_stdint_probe.err"; \
	printf '%s\n%s\n%s\n%s\n' '#include <stdint.h>' 'int main(void){' '    return (sizeof(int32_t) == 4 && sizeof(uint64_t) == 8 && sizeof(intptr_t) == sizeof(void *)) ? 42 : 1;' '}' > "$$src"; \
	echo "--- Non-boot stdint smoke test ---"; \
	if ! $(STAGE1) -std=c11 "$$src" -o "$$exe" >"$$err" 2>&1; then \
		echo "  FAIL <stdint.h> C11 compile"; \
		sed 's/^/    /' "$$err"; \
		exit 1; \
	fi; \
	set +e; "$$exe" >/dev/null 2>>"$$err"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stdint.h> C11 runtime"; \
	else \
		echo "  FAIL <stdint.h> C11 runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo ""

test-nonboot-stdatomic: $(STAGE1) | $(TMP_DIR)
	@set -u; \
	src="$(TMP_DIR)/nonboot_stdatomic_probe.c"; \
	exe="$(TMP_DIR)/nonboot_stdatomic_probe"; \
	err="$(TMP_DIR)/nonboot_stdatomic_probe.err"; \
	printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
		'#include <stdatomic.h>' \
		'' \
		'int main(void) {' \
		'    atomic_int value = ATOMIC_VAR_INIT(1);' \
		'    atomic_store(&value, 7);' \
		'    atomic_init(&value, 11);' \
		'    return atomic_load(&value) == 11 ? 42 : 1;' \
		'}' > "$$src"; \
	echo "--- Non-boot stdatomic smoke test ---"; \
	if ! $(STAGE1) -std=c11 "$$src" -o "$$exe" >"$$err" 2>&1; then \
		echo "  FAIL <stdatomic.h> C11 compile"; \
		sed 's/^/    /' "$$err"; \
		exit 1; \
	fi; \
	set +e; "$$exe" >/dev/null 2>>"$$err"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stdatomic.h> C11 runtime"; \
	else \
		echo "  FAIL <stdatomic.h> C11 runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo ""

test-nonboot-system-headers: $(STAGE1) | $(TMP_DIR)
	@set -u; \
	src="$(TMP_DIR)/nonboot_system_headers_probe.c"; \
	exe="$(TMP_DIR)/nonboot_system_headers_probe"; \
	err="$(TMP_DIR)/nonboot_system_headers_probe.err"; \
	printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
		'#include <dlfcn.h>' \
		'#include <sys/types.h>' \
		'#include <sys/stat.h>' \
		'' \
		'int main(void) {' \
		'    dev_t dev = (dev_t)0;' \
		'    ino_t ino = (ino_t)0;' \
		'    struct stat st;' \
		'    void *(*open_fn)(const char *, int) = dlopen;' \
		'    int (*close_fn)(void *) = dlclose;' \
		'    char *(*err_fn)(void) = dlerror;' \
		'    return sizeof(dev) > 0 && sizeof(ino) > 0 && sizeof(st.st_mode) > 0 && open_fn && close_fn && err_fn ? 42 : 1;' \
		'}' > "$$src"; \
	echo "--- Non-boot system header smoke test ---"; \
	if ! $(STAGE1) -std=c11 "$$src" -o "$$exe" >"$$err" 2>&1; then \
		echo "  FAIL Darwin system header compile"; \
		sed 's/^/    /' "$$err"; \
		exit 1; \
	fi; \
	set +e; "$$exe" >/dev/null 2>>"$$err"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS Darwin system header runtime"; \
	else \
		echo "  FAIL Darwin system header runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo ""

# ---------------------------------------------------------------------------
# Debug / generation helpers
# ---------------------------------------------------------------------------

dump-ast: $(STAGE0)
	$(STAGE0) -dump-ast $(TEST_DIR)/core/test008.c

test-cfg: $(STAGE0)
	@echo "Dumping CFG smoke test:"
	@$(STAGE0) -dump-cfg tests/control/test001.c | head -20

smoketest: $(STAGE1) | $(SELFHOST_DIR)
	@echo "Smoke test:"
	@echo "  $(STAGE1) -S -target=$(SELFHOST_TARGET) $(SRC_DIR)/lexer.c -o $(SELFHOST_DIR)/smoke.s"
	@$(STAGE1) -S -target=$(SELFHOST_TARGET) $(SRC_DIR)/lexer.c -o $(SELFHOST_DIR)/smoke.s
	@echo "Stage1 smoke test OK  ($(SELFHOST_DIR)/smoke.s)"

arm64: $(STAGE0) | $(TMP_DIR)
	@while IFS= read -r entry; do \
		case "$$entry" in ''|\#*) continue ;; esac; \
		t=$${entry%%:*}; base=$${t%.c}; \
		echo "Generating/running $$base"; \
		$(STAGE0) -S -target=arm64 $(TEST_DIR)/$$t > $(TMP_DIR)/out.s; \
		clang $(TMP_DIR)/out.s -o $(TMP_DIR)/out; \
		$(TMP_DIR)/out; echo "exit: $$?"; \
	done < $(TEST_DIR)/manifest.txt

x86: $(STAGE0)
	@while IFS= read -r entry; do \
		case "$$entry" in ''|\#*) continue ;; esac; \
		t=$${entry%%:*}; base=$${t%.c}; \
		$(STAGE0) -target=x86 $(TEST_DIR)/$$t > $$base.asm; \
		echo "Generated $$base.asm"; \
	done < $(TEST_DIR)/manifest.txt

X86_AGGREGATE_SMOKE_TESTS = \
	$(TEST_DIR)/backend/x86_local_struct_fields.c \
	$(TEST_DIR)/backend/x86_nested_struct_fields.c \
	$(TEST_DIR)/backend/x86_array_of_structs.c \
	$(TEST_DIR)/backend/x86_struct_ptr_fields.c \
	$(TEST_DIR)/backend/x86_funcptr_struct.c \
	$(TEST_DIR)/backend/x86_narrow_struct_fields.c

test-x86-smoke: $(STAGE0) test-x86-aggregate-smoke | $(TMP_DIR)
	@sh $(TEST_DIR)/backend/x86_smoke.sh -c $(STAGE0) -T $(TMP_DIR) -d $(TEST_DIR)

# Extra x86 ABI smoke coverage kept as a separate prerequisite so it does not
# depend on the internal layout of tests/backend/x86_smoke.sh.
test-x86-aggregate-smoke: $(STAGE0) | $(TMP_DIR)
	@set -u; \
	fail=0; \
	echo "x86 aggregate assembly smoke test:"; \
	for src in $(X86_AGGREGATE_SMOKE_TESTS); do \
		name=$$(basename "$$src" .c); \
		asm="$(TMP_DIR)/$$name.x86.s"; \
		err="$(TMP_DIR)/$$name.x86.err"; \
		if $(STAGE0) $(BOOT_FLAGS) -target=x86 -asm=gas -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  PASS $$name"; \
		else \
			echo "  FAIL $$name"; \
			sed 's/^/    /' "$$err"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	if [ "$$fail" -ne 0 ]; then \
		echo "x86 aggregate assembly smoke test FAILED ($$fail failure(s))"; \
		exit "$$fail"; \
	fi; \
	echo "x86 aggregate assembly smoke test OK"

test-x64: test-x64-smoke

test-x64-smoke: $(STAGE0) | $(TMP_DIR)
	@sh $(TEST_DIR)/backend/x64_smoke.sh -c $(STAGE0) -T $(TMP_DIR) -d $(TEST_DIR)

# Execute focused x64 _Complex ABI probes.  This is kept independent from the
# broader x64 backend run target while that target still contains unrelated
# known failures.
test-x64-complex-run: $(STAGE0) | $(TMP_DIR)
	@set -u; \
	TMP="$(TMP_DIR)/x64-complex-run"; \
	mkdir -p "$$TMP"; \
	fail=0; \
	echo "x64 complex ABI execution test:"; \
	run() { \
		src="$$1"; extra="$$2"; expect="$$3"; name=$$(basename "$$src" .c); \
		asm="$$TMP/$$name.s"; exe="$$TMP/$$name"; err="$$TMP/$$name.err"; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=x64 -std=c11 -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  FAIL $$name (compile)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		if ! $(X64_RUN_CC) $(X64_RUN_LDFLAGS) "$$asm" $$extra -o "$$exe" >>"$$err" 2>&1; then \
			echo "  FAIL $$name (link)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		set +e; "$$exe"; got="$$?"; set -e; \
		if [ "$$got" -eq "$$expect" ]; then echo "  PASS $$name"; else echo "  FAIL $$name (exit $$got, expected $$expect)"; fail=$$((fail + 1)); fi; \
	}; \
	run_tcc_callee() { \
		src="$$1"; caller="$$2"; expect="$$3"; name=$$(basename "$$src" .c); \
		asm="$$TMP/$$name.s"; exe="$$TMP/$$name"; err="$$TMP/$$name.err"; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=x64 -std=c11 -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  FAIL $$name (compile)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		if ! $(X64_RUN_CC) $(X64_RUN_LDFLAGS) "$$caller" "$$asm" -o "$$exe" >>"$$err" 2>&1; then \
			echo "  FAIL $$name (link)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		set +e; "$$exe"; got="$$?"; set -e; \
		if [ "$$got" -eq "$$expect" ]; then echo "  PASS $$name"; else echo "  FAIL $$name (exit $$got, expected $$expect)"; fail=$$((fail + 1)); fi; \
	}; \
	run tests/abi/test127_complex_float2.c "" 42; \
	run tests/abi/test128_complex_double2.c "" 42; \
	run tests/abi/test131_complex_mixed_args.c "" 42; \
	run tests/abi/test130_complex_double2_external.c tests/abi/lib_test130_complex_double2_external.c 42; \
	run tests/abi/test132_complex_mixed_args_external.c tests/abi/lib_test132_complex_mixed_args_external.c 42; \
	run tests/abi/test142_complex_double_variadic_tail.c "" 42; \
	run tests/abi/test143_complex_float_variadic_tail.c "" 42; \
	run tests/abi/test144_complex_variadic_external.c tests/abi/lib_test144_complex_variadic_external.c 42; \
	run_tcc_callee tests/abi/test145_complex_double_variadic_overflow.c tests/abi/lib_test145_complex_double_variadic_overflow.c 42; \
	run_tcc_callee tests/abi/test146_complex_float_variadic_overflow.c tests/abi/lib_test146_complex_float_variadic_overflow.c 42; \
	run tests/abi/test147_integer_variadic_overflow.c "" 42; \
	run tests/abi/test148_va_copy.c "" 42; \
	run tests/abi/test149_complex_scalar_fp_mixed_external.c tests/abi/lib_test149_complex_scalar_fp_mixed_external.c 42; \
	run tests/abi/test150_complex_double_xmm_spill_external.c tests/abi/lib_test150_complex_double_xmm_spill_external.c 42; \
	run tests/abi/test151_complex_double_xmm_spill_mixed_external.c tests/abi/lib_test151_complex_double_xmm_spill_mixed_external.c 42; \
	run_tcc_callee tests/abi/test152_complex_double_xmm_spill_callee.c tests/abi/lib_test152_complex_double_xmm_spill_callee.c 42; \
	run tests/abi/test153_complex_float_xmm_spill_external.c tests/abi/lib_test153_complex_float_xmm_spill_external.c 42; \
	if [ "$$fail" -eq 0 ]; then echo "x64 complex ABI execution test OK"; else echo "x64 complex ABI execution test FAILED ($$fail failure(s))"; exit "$$fail"; fi

# Execute a conservative subset of x64 backend tests. This is separate from
# test-x64-smoke because it requires a host/toolchain that can link and run
# x86_64 binaries. On Apple Silicon with Rosetta installed, use:
#
#   make test-x64-run X64_RUN_LDFLAGS="-arch x86_64"
#
# Exit-code expectations are compared modulo 256, matching process status
# semantics for tests whose C return value is larger than 255.
test-x64-run: $(STAGE0) | $(TMP_DIR)
	@set -u; \
	TMP="$(TMP_DIR)/x64-run"; \
	mkdir -p "$$TMP"; \
	fail=0; \
	echo "x64 execution smoke test:"; \
	run() { \
		src="$$1"; expect="$$2"; name=$$(basename "$$src" .c); \
		asm="$$TMP/$$name.s"; exe="$$TMP/$$name"; err="$$TMP/$$name.err"; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=x64 -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  FAIL $$name (compile)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		if ! $(X64_RUN_CC) $(X64_RUN_LDFLAGS) "$$asm" -o "$$exe" >>"$$err" 2>&1; then \
			echo "  FAIL $$name (link)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		set +e; "$$exe"; got="$$?"; set -e; \
		if [ "$$got" -eq "$$expect" ]; then \
			echo "  PASS $$name"; \
		else \
			echo "  FAIL $$name (exit $$got, expected $$expect)"; fail=$$((fail + 1)); \
		fi; \
	}; \
	run tests/backend/x64_funcptr_basic.c 42; \
	run tests/backend/x64_funcptr_global.c 42; \
	run tests/abi/test127_complex_float2.c 42; \
	run tests/abi/test128_complex_double2.c 42; \
	run tests/abi/test131_complex_mixed_args.c 42; \
	run tests/backend/x64_funcptr_struct.c 42; \
	run tests/backend/x64_funcptr_reassign.c 43; \
	run tests/backend/x64_indirect_call_args.c 28; \
	run tests/backend/x64_call_args.c 140; \
	run tests/backend/x64_addr_of_local.c 42; \
	run tests/backend/x64_pointer_arith.c 30; \
	run tests/backend/x64_pointer_inc_store.c 41; \
	run tests/backend/x64_pre_post_inc.c 34; \
	run tests/backend/x64_local_array_index.c 30; \
	run tests/backend/x64_local_struct_fields.c 211; \
	run tests/backend/x64_struct_ptr_fields.c 42; \
	run tests/backend/x64_struct_arg_by_value.c 42; \
	run tests/backend/x64_struct_return_small.c 42; \
	run tests/backend/x64_array_of_structs.c 0; \
	run tests/backend/x64_struct_arg_mixed_scalars.c 0; \
	run tests/backend/x64_struct_arg_large.c 0; \
	run tests/backend/x64_struct_return_medium.c 0; \
	run tests/backend/x64_i64_args_return_mix.c 11; \
	run tests/backend/x64_many_args.c 129; \
	run tests/backend/x64_stack_alignment_call.c 0; \
	run tests/backend/x64_narrow_struct_fields.c 18; \
	run tests/backend/x64_nested_struct_return_call.c 55; \
	run tests/backend/x64_struct_return_with_args.c 80; \
	run tests/backend/x64_mixed_width_locals.c 211; \
	run tests/backend/x64_narrow_load_store.c 51; \
	run tests/backend/x64_signed_unsigned_cmp.c 44; \
	run tests/backend/x64_div_mod_signed.c 173; \
	run tests/backend/x64_div_mod_unsigned.c 83; \
	run tests/backend/x64_i64_add_sub.c 42; \
	run tests/backend/x64_i64_compare.c 11; \
	run tests/backend/x64_i64_mul_div_mod.c 16; \
	run tests/backend/x64_i64_shifts.c 0; \
	run tests/backend/x64_i64_bitwise.c 0; \
	run tests/backend/x64_casts_int_uint_i64_ptr.c 0; \
	run tests/backend/x64_nested_struct_fields.c 0; \
	run tests/backend/x64_varargs_sum.c 100; \
	run tests/backend/x64_varargs_many.c 0; \
	run tests/backend/x64_varargs_mixed.c 42; \
	run tests/backend/x64_conditional_expr.c 42; \
	run tests/backend/x64_switch_basic.c 50; \
	run tests/backend/x64_short_circuit.c 22; \
	run tests/backend/x64_nested_break_continue.c 25; \
	run tests/backend/x64_recursive_calls.c 120; \
	run tests/backend/x64_nested_call_args.c 42; \
	run tests/backend/x64_mutual_calls.c 1; \
	run tests/backend/x64_static_locals.c 42; \
	run tests/backend/x64_global_arrays.c 23; \
	run tests/backend/x64_global_initializers.c 226; \
	run tests/backend/x64_global_struct_fields.c 211; \
	run tests/backend/x64_string_global_ptr.c 101; \
	if [ "$$fail" -eq 0 ]; then \
		echo "x64 execution smoke test OK"; \
	else \
		echo "x64 execution smoke test FAILED ($$fail failure(s))"; \
		exit "$$fail"; \
	fi

# Execute a conservative subset of x86 backend tests when the host can build
# and run 32-bit x86 binaries. If the host lacks 32-bit compiler/linker/runtime
# support, this target prints SKIP and succeeds so it remains safe inside
# aggregate test gates on Apple Silicon and other non-ILP32 hosts.
test-x86-run: $(STAGE0) | $(TMP_DIR)
	@set -u; \
	TMP="$(TMP_DIR)/x86-run"; \
	mkdir -p "$$TMP"; \
	echo "x86 execution smoke test:"; \
	printf '%s\n' 'int main(void) { return 0; }' > "$$TMP/probe.c"; \
	if ! $(X86_RUN_CC) $(X86_RUN_CFLAGS) "$$TMP/probe.c" -o "$$TMP/probe" >"$$TMP/probe.err" 2>&1; then \
		echo "  SKIP host cannot compile/link 32-bit x86 binaries with $(X86_RUN_CC) $(X86_RUN_CFLAGS)"; \
		if [ "$(X86_RUN_VERBOSE_SKIP)" = "1" ]; then sed 's/^/    /' "$$TMP/probe.err"; fi; \
		exit 0; \
	fi; \
	if ! "$$TMP/probe" >"$$TMP/probe.run.out" 2>"$$TMP/probe.run.err"; then \
		echo "  SKIP host cannot execute 32-bit x86 binaries"; \
		if [ "$(X86_RUN_VERBOSE_SKIP)" = "1" ]; then sed 's/^/    /' "$$TMP/probe.run.err"; fi; \
		exit 0; \
	fi; \
	fail=0; \
	run() { \
		src="$$1"; expect="$$2"; name=$$(basename "$$src" .c); \
		asm="$$TMP/$$name.s"; exe="$$TMP/$$name"; err="$$TMP/$$name.err"; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=x86 -asm=gas -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  FAIL $$name (compile)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		if ! $(X86_RUN_CC) $(X86_RUN_LDFLAGS) "$$asm" -o "$$exe" >>"$$err" 2>&1; then \
			echo "  FAIL $$name (link)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		set +e; "$$exe"; got="$$?"; set -e; \
		if [ "$$got" -eq "$$expect" ]; then \
			echo "  PASS $$name"; \
		else \
			echo "  FAIL $$name (exit $$got, expected $$expect)"; fail=$$((fail + 1)); \
		fi; \
	}; \
	run tests/backend/x86_call_args.c 52; \
	run tests/backend/x86_signed_unsigned_cmp.c 2; \
	run tests/backend/x86_div_mod_signed.c 240; \
	run tests/backend/x86_div_mod_unsigned.c 16; \
	run tests/backend/x86_addr_of_local.c 42; \
	run tests/backend/x86_pointer_arith.c 42; \
	run tests/backend/x86_pointer_inc_store.c 6; \
	run tests/backend/x86_pre_post_inc.c 34; \
	run tests/backend/x86_local_array_index.c 35; \
	run tests/backend/x86_local_struct_fields.c 42; \
	run tests/backend/x86_struct_ptr_fields.c 42; \
	run tests/backend/x86_array_of_structs.c 0; \
	run tests/backend/x86_nested_struct_fields.c 0; \
	run tests/backend/x86_struct_arg_by_value.c 42; \
	run tests/backend/x86_struct_arg_mixed_scalars.c 0; \
	run tests/backend/x86_struct_return_small.c 42; \
	run tests/backend/x86_struct_return_medium.c 0; \
	run tests/backend/x86_funcptr_basic.c 42; \
	run tests/backend/x86_funcptr_reassign.c 42; \
	run tests/backend/x86_funcptr_global.c 42; \
	run tests/backend/x86_funcptr_struct.c 42; \
	run tests/backend/x86_indirect_call_args.c 42; \
	run tests/backend/x86_conditional_expr.c 17; \
	run tests/backend/x86_switch_basic.c 50; \
	run tests/backend/x86_short_circuit.c 0; \
	run tests/backend/x86_nested_break_continue.c 0; \
	run tests/backend/x86_recursive_calls.c 0; \
	run tests/backend/x86_nested_call_args.c 0; \
	run tests/backend/x86_mutual_calls.c 0; \
	run tests/backend/x86_static_locals.c 42; \
	run tests/backend/x86_global_arrays.c 42; \
	run tests/backend/x86_global_initializers.c 50; \
	run tests/backend/x86_global_struct_fields.c 42; \
	run tests/backend/x86_string_global_ptr.c 42; \
	run tests/backend/x86_mixed_width_locals.c 56; \
	run tests/backend/x86_i64_add_sub.c 27; \
	run tests/backend/x86_i64_compare.c 11; \
	run tests/backend/x86_i64_mul_div_mod.c 16; \
	run tests/backend/x86_i64_shifts.c 0; \
	run tests/backend/x86_i64_bitwise.c 0; \
	run tests/backend/x86_casts_int_uint_i64_ptr.c 0; \
	run tests/backend/x86_varargs_sum.c 60; \
	run tests/backend/x86_varargs_mixed.c 15; \
	if [ "$$fail" -eq 0 ]; then \
		echo "x86 execution smoke test OK"; \
	else \
		echo "x86 execution smoke test FAILED ($$fail failure(s))"; \
		exit "$$fail"; \
	fi

test-mips-run: $(STAGE0) | $(TMP_DIR)
	@set -u; \
	TMP="$(TMP_DIR)/mips-run"; \
	mkdir -p "$$TMP"; \
	echo "MIPS execution smoke test:"; \
	if ! command -v $(MIPS_RUN_CC) >/dev/null 2>&1; then \
		echo "  SKIP missing MIPS cross compiler: $(MIPS_RUN_CC)"; \
		exit 0; \
	fi; \
	if ! command -v $(MIPS_RUN_QEMU) >/dev/null 2>&1; then \
		echo "  SKIP missing MIPS emulator: $(MIPS_RUN_QEMU)"; \
		exit 0; \
	fi; \
	printf '%s\n' 'int main(void) { return 0; }' > "$$TMP/probe.c"; \
	if ! $(MIPS_RUN_CC) $(MIPS_RUN_LDFLAGS) "$$TMP/probe.c" -o "$$TMP/probe" >"$$TMP/probe.err" 2>&1; then \
		echo "  SKIP host cannot compile/link MIPS binaries with $(MIPS_RUN_CC) $(MIPS_RUN_LDFLAGS)"; \
		if [ "$(MIPS_RUN_VERBOSE_SKIP)" = "1" ]; then sed 's/^/    /' "$$TMP/probe.err"; fi; \
		exit 0; \
	fi; \
	if ! $(MIPS_RUN_QEMU) $(MIPS_RUN_QEMU_ARGS) "$$TMP/probe" >"$$TMP/probe.run.out" 2>"$$TMP/probe.run.err"; then \
		echo "  SKIP host cannot execute MIPS binaries with $(MIPS_RUN_QEMU) $(MIPS_RUN_QEMU_ARGS)"; \
		if [ "$(MIPS_RUN_VERBOSE_SKIP)" = "1" ]; then sed 's/^/    /' "$$TMP/probe.run.err"; fi; \
		exit 0; \
	fi; \
	fail=0; \
	run() { \
		src="$$1"; expect="$$2"; name=$$(basename "$$src" .c); \
		asm="$$TMP/$$name.s"; exe="$$TMP/$$name"; err="$$TMP/$$name.err"; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=mips -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  FAIL $$name (compile)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		if ! $(MIPS_RUN_CC) $(MIPS_RUN_LDFLAGS) "$$asm" -o "$$exe" >>"$$err" 2>&1; then \
			echo "  FAIL $$name (link)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		set +e; $(MIPS_RUN_QEMU) $(MIPS_RUN_QEMU_ARGS) "$$exe"; got="$$?"; set -e; \
		if [ "$$got" -eq "$$expect" ]; then \
			echo "  PASS $$name"; \
		else \
			echo "  FAIL $$name (exit $$got, expected $$expect)"; fail=$$((fail + 1)); \
		fi; \
	}; \
	run tests/backend/mips_call_args.c 55; \
	run tests/backend/mips_global_initializers.c 91; \
	run tests/backend/mips_local_stack.c 42; \
	run tests/backend/mips_branch_loop.c 7; \
	run tests/backend/mips_pointer_load_store.c 42; \
	run tests/backend/mips_static_locals.c 0; \
	run tests/backend/mips_global_arrays.c 0; \
	run tests/backend/mips_pointer_arith.c 0; \
	run tests/backend/mips_signed_unsigned_cmp.c 0; \
	run tests/backend/mips_short_circuit.c 0; \
	run tests/backend/mips_funcptr_basic.c 0; \
	run tests/backend/mips_funcptr_reassign.c 0; \
	run tests/backend/mips_funcptr_global.c 0; \
	run tests/backend/mips_indirect_call_args.c 60; \
	run tests/backend/mips_funcptr_struct.c 42; \
	run tests/backend/mips_recursive_calls.c 0; \
	run tests/backend/mips_nested_call_args.c 0; \
	run tests/backend/mips_array_of_structs.c 42; \
	run tests/backend/mips_nested_struct_fields.c 42; \
	run tests/backend/mips_struct_arg_by_value.c 72; \
	run tests/backend/mips_struct_arg_mixed_scalars.c 42; \
	run tests/backend/mips_struct_return_small.c 42; \
	run tests/backend/mips_struct_return_medium.c 42; \
	run tests/backend/mips_varargs_sum.c 42; \
	run tests/backend/mips_varargs_mixed.c 42; \
	if [ "$$fail" -eq 0 ]; then \
		echo "MIPS execution smoke test OK"; \
	else \
		echo "MIPS execution smoke test FAILED ($$fail failure(s))"; \
		exit "$$fail"; \
	fi

test-m68k-run: $(STAGE0) | $(TMP_DIR)
	@set -u; \
	TMP="$(TMP_DIR)/m68k-run"; \
	mkdir -p "$$TMP"; \
	echo "m68k execution smoke test:"; \
	if ! command -v $(M68K_RUN_CC) >/dev/null 2>&1; then \
		echo "  SKIP missing m68k cross compiler: $(M68K_RUN_CC)"; \
		exit 0; \
	fi; \
	if ! command -v $(M68K_RUN_QEMU) >/dev/null 2>&1; then \
		echo "  SKIP missing m68k emulator: $(M68K_RUN_QEMU)"; \
		exit 0; \
	fi; \
	printf '%s\n' 'int main(void) { return 0; }' > "$$TMP/probe.c"; \
	if ! $(M68K_RUN_CC) $(M68K_RUN_LDFLAGS) "$$TMP/probe.c" -o "$$TMP/probe" >"$$TMP/probe.err" 2>&1; then \
		echo "  SKIP host cannot compile/link m68k binaries with $(M68K_RUN_CC) $(M68K_RUN_LDFLAGS)"; \
		if [ "$(M68K_RUN_VERBOSE_SKIP)" = "1" ]; then sed 's/^/    /' "$$TMP/probe.err"; fi; \
		exit 0; \
	fi; \
	if ! $(M68K_RUN_QEMU) $(M68K_RUN_QEMU_ARGS) "$$TMP/probe" >"$$TMP/probe.run.out" 2>"$$TMP/probe.run.err"; then \
		echo "  SKIP host cannot execute m68k binaries with $(M68K_RUN_QEMU) $(M68K_RUN_QEMU_ARGS)"; \
		if [ "$(M68K_RUN_VERBOSE_SKIP)" = "1" ]; then sed 's/^/    /' "$$TMP/probe.run.err"; fi; \
		exit 0; \
	fi; \
	fail=0; \
	run() { \
		src="$$1"; expect="$$2"; name=$$(basename "$$src" .c); \
		asm="$$TMP/$$name.s"; exe="$$TMP/$$name"; err="$$TMP/$$name.err"; \
		if ! $(STAGE0) $(BOOT_FLAGS) -target=m68k -mcpu=68000 -S "$$src" -o "$$asm" >"$$err" 2>&1; then \
			echo "  FAIL $$name (compile)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		if ! $(M68K_RUN_CC) $(M68K_RUN_LDFLAGS) "$$asm" -o "$$exe" >>"$$err" 2>&1; then \
			echo "  FAIL $$name (link)"; sed 's/^/    /' "$$err"; fail=$$((fail + 1)); return; \
		fi; \
		set +e; $(M68K_RUN_QEMU) $(M68K_RUN_QEMU_ARGS) "$$exe"; got="$$?"; set -e; \
		if [ "$$got" -eq "$$expect" ]; then \
			echo "  PASS $$name"; \
		else \
			echo "  FAIL $$name (exit $$got, expected $$expect)"; fail=$$((fail + 1)); \
		fi; \
	}; \
	run tests/backend/m68k_call_probe.c 42; \
	run tests/backend/m68k_global.c 42; \
	run tests/backend/m68k_local.c 42; \
	run tests/backend/m68k_sub.c 42; \
	run tests/backend/m68k_switch_basic.c 42; \
	run tests/backend/m68k_funcptr_struct.c 42; \
	run tests/backend/m68k_addr_of_local.c 0; \
	run tests/backend/m68k_array_of_structs.c 0; \
	run tests/backend/m68k_branch_loop.c 0; \
	run tests/backend/m68k_compare.c 0; \
	run tests/backend/m68k_conditional_expr.c 0; \
	run tests/backend/m68k_div_mod_signed.c 0; \
	run tests/backend/m68k_div_mod_unsigned.c 0; \
	run tests/backend/m68k_funcptr_basic.c 0; \
	run tests/backend/m68k_funcptr_global.c 0; \
	run tests/backend/m68k_funcptr_reassign.c 0; \
	run tests/backend/m68k_global_arrays.c 0; \
	run tests/backend/m68k_global_struct_fields.c 0; \
	run tests/backend/m68k_indirect_call_args.c 0; \
	run tests/backend/m68k_local_array_index.c 0; \
	run tests/backend/m68k_local_struct_fields.c 0; \
	run tests/backend/m68k_mixed_width_locals.c 0; \
	run tests/backend/m68k_mutual_calls.c 0; \
	run tests/backend/m68k_narrow_struct_fields.c 0; \
	run tests/backend/m68k_nested_break_continue.c 0; \
	run tests/backend/m68k_nested_call_args.c 0; \
	run tests/backend/m68k_nested_struct_fields.c 0; \
	run tests/backend/m68k_pointer_inc_store.c 0; \
	run tests/backend/m68k_pointer_load_store.c 0; \
	run tests/backend/m68k_pre_post_inc.c 0; \
	run tests/backend/m68k_recursive_calls.c 0; \
	run tests/backend/m68k_short_circuit.c 0; \
	run tests/backend/m68k_signed_unsigned_cmp.c 0; \
	run tests/backend/m68k_static_locals.c 0; \
	run tests/backend/m68k_string_global_ptr.c 0; \
	run tests/backend/m68k_struct_ptr_fields.c 0; \
	if [ "$$fail" -eq 0 ]; then \
		echo "m68k execution smoke test OK"; \
	else \
		echo "m68k execution smoke test FAILED ($$fail failure(s))"; \
		exit "$$fail"; \
	fi

test-backend-run: $(BACKEND_RUN_TARGETS)

test-mips: test-mips-smoke

test-mips-smoke: $(STAGE0) | $(TMP_DIR)
	@sh $(TEST_DIR)/backend/mips_smoke.sh -c $(STAGE0) -T $(TMP_DIR) -d $(TEST_DIR)

test-m68k-smoke:
	CC=$(STAGE0) sh tests/backend/m68k_smoke.sh


test-backend-smoke: $(BACKEND_SMOKE_TARGETS)

test-debug-sections: $(STAGE0) | $(TMP_DIR)
	@sh $(TEST_DIR)/debug/debug_sections_smoke.sh -c $(STAGE0) -T $(TMP_DIR) -d $(TEST_DIR)

test-installed-smoke:
	@set -eu; \
	compiler="$(BINDIR)/tcc"; \
	if [ ! -x "$$compiler" ]; then \
		echo "Installed compiler not found: $$compiler"; \
		echo "Run 'make install' first or override PREFIX/BINDIR."; \
		exit 1; \
	fi; \
	workdir="$$(mktemp -d /tmp/tcc-installed-smoke.XXXXXX)"; \
	trap 'rm -rf "$$workdir"' EXIT INT TERM HUP; \
	echo "Installed compiler smoke test:"; \
	echo "  compiler: $$compiler"; \
	echo "  workdir:  $$workdir"; \
	src_bool_nohdr="$$workdir/stdbool_no_header.c"; \
	src_bool="$$workdir/stdbool_probe.c"; \
	exe_bool="$$workdir/stdbool_probe"; \
	err_bool_nohdr="$$workdir/stdbool_no_header.err"; \
	err_bool="$$workdir/stdbool_probe.err"; \
	printf '%s\n' 'bool x = true; int main(void){ return x ? 42 : 1; }' > "$$src_bool_nohdr"; \
	printf '%s\n%s\n' '#include <stdbool.h>' 'bool x = true; int main(void){ return x ? 42 : 1; }' > "$$src_bool"; \
	if "$$compiler" -std=c11 "$$src_bool_nohdr" -o "$$workdir/stdbool_no_header" >"$$err_bool_nohdr" 2>&1; then \
		echo "  FAIL bool accepted without <stdbool.h> in C11"; \
		exit 1; \
	else \
		echo "  PASS bool requires <stdbool.h> in C11"; \
	fi; \
	if ! "$$compiler" -std=c11 "$$src_bool" -o "$$exe_bool" >"$$err_bool" 2>&1; then \
		echo "  FAIL <stdbool.h> installed-compiler compile"; \
		sed 's/^/    /' "$$err_bool"; \
		exit 1; \
	fi; \
	set +e; "$$exe_bool" >/dev/null 2>>"$$err_bool"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS <stdbool.h> installed-compiler runtime"; \
	else \
		echo "  FAIL <stdbool.h> installed-compiler runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	src_headers="$$workdir/c11_headers_probe.c"; \
	exe_headers="$$workdir/c11_headers_probe"; \
	err_headers="$$workdir/c11_headers_probe.err"; \
	printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
		'#include <stdalign.h>' \
		'#include <stdatomic.h>' \
		'#include <stdint.h>' \
		'#include <stddef.h>' \
		'' \
		'alignas(16) static int aligned_value;' \
		'int main(void) {' \
		'    atomic_int value = ATOMIC_VAR_INIT(3); atomic_store(&value, 11);' \
		'    return alignof(int) == 4 && sizeof(int32_t) == 4 && sizeof(uint64_t) == 8 && sizeof(nullptr_t) == sizeof(void *) && atomic_load(&value) == 11 && aligned_value == 0 ? 42 : 1; }' \
		> "$$src_headers"; \
	if ! "$$compiler" -std=c23 "$$src_headers" -o "$$exe_headers" >"$$err_headers" 2>&1; then \
		echo "  FAIL installed standard-header compile"; \
		sed 's/^/    /' "$$err_headers"; \
		exit 1; \
	fi; \
	set +e; "$$exe_headers" >/dev/null 2>>"$$err_headers"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS installed standard-header runtime"; \
	else \
		echo "  FAIL installed standard-header runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	src_sys="$$workdir/system_headers_probe.c"; \
	exe_sys="$$workdir/system_headers_probe"; \
	err_sys="$$workdir/system_headers_probe.err"; \
	printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
		'#include <dlfcn.h>' \
		'#include <sys/types.h>' \
		'#include <sys/stat.h>' \
		'' \
		'int main(void) {' \
		'    dev_t dev = (dev_t)0;' \
		'    ino_t ino = (ino_t)0;' \
		'    struct stat st;' \
		'    void *(*open_fn)(const char *, int) = dlopen;' \
		'    int (*close_fn)(void *) = dlclose;' \
		'    char *(*err_fn)(void) = dlerror;' \
		'    return sizeof(dev) > 0 && sizeof(ino) > 0 && sizeof(st.st_mode) > 0 && open_fn && close_fn && err_fn ? 42 : 1;' \
		'}' > "$$src_sys"; \
	if ! "$$compiler" -std=c11 "$$src_sys" -o "$$exe_sys" >"$$err_sys" 2>&1; then \
		echo "  FAIL installed system-header compile"; \
		sed 's/^/    /' "$$err_sys"; \
		exit 1; \
	fi; \
	set +e; "$$exe_sys" >/dev/null 2>>"$$err_sys"; status="$$?"; set -e; \
	if [ "$$status" -eq 42 ]; then \
		echo "  PASS installed system-header runtime"; \
	else \
		echo "  FAIL installed system-header runtime (exit $$status, expected 42)"; \
		exit 1; \
	fi; \
	echo "Installed compiler smoke test OK"

SQLITE_SMOKE_DIR ?= $(HOME)/Projects/Programming/c/sqlite
SQLITE_SMOKE_COMPILER ?= $(BINDIR)/tcc
SQLITE_SMOKE_LIBS ?= -lpthread
SQLITE_REPORT_LIMIT ?= 40
SQLITE_REPORT_MIN_PERCENT ?= 0

test-sqlite-smoke:
	@set -eu; \
	compiler="$(SQLITE_SMOKE_COMPILER)"; \
	sqlite_dir="$(SQLITE_SMOKE_DIR)"; \
	if [ ! -x "$$compiler" ]; then \
		echo "SQLite smoke compiler not found: $$compiler"; \
		echo "Run 'make install' first or override SQLITE_SMOKE_COMPILER."; \
		exit 1; \
	fi; \
	if [ ! -f "$$sqlite_dir/main.c" ] || [ ! -f "$$sqlite_dir/sqlite3.c" ]; then \
		echo "SQLite smoke sources not found under: $$sqlite_dir"; \
		echo "Override SQLITE_SMOKE_DIR to point at a sqlite amalgamation tree."; \
		exit 1; \
	fi; \
	workdir="$$(mktemp -d /tmp/tcc-sqlite-smoke.XXXXXX)"; \
	trap 'rm -rf "$$workdir"' EXIT INT TERM HUP; \
	exe="$$workdir/sqlite-smoke"; \
	err="$$workdir/sqlite-smoke.err"; \
	echo "SQLite smoke test:"; \
	echo "  compiler: $$compiler"; \
	echo "  sources:  $$sqlite_dir"; \
	echo "  workdir:  $$workdir"; \
	if ! (cd "$$sqlite_dir" && "$$compiler" main.c sqlite3.c $(SQLITE_SMOKE_LIBS) -o "$$exe") >"$$err" 2>&1; then \
		echo "  FAIL sqlite compile/link"; \
		sed 's/^/    /' "$$err"; \
		exit 1; \
	fi; \
	set +e; "$$exe" >/dev/null 2>>"$$err"; status="$$?"; set -e; \
	if [ "$$status" -eq 0 ]; then \
		echo "  PASS sqlite runtime"; \
	else \
		echo "  FAIL sqlite runtime (exit $$status, expected 0)"; \
		sed 's/^/    /' "$$err"; \
		exit 1; \
	fi; \
	echo "SQLite smoke test OK"

test-release-gates-core:
	@set -eu; \
	$(MAKE) stage2; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-conformance-c99; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-conformance-c11; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-conformance-c17; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-conformance-c23; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-conformance-external-torture; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-conformance-external-ctestsuite-scc; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_CORE) test-sanitize

test-release-gates-installed:
	@set -eu; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_INSTALLED) test-installed-smoke; \
	$(MAKE) TMP_DIR=$(TMP_DIR_RELEASE_INSTALLED) test-sqlite-smoke

test-release-gates: test-release-gates-core test-release-gates-installed

report-selfhost-func-sizes: $(STAGE0) $(STAGE1)
	@echo "Self-host function-size report (stage0 vs stage1):"
	@python3 tools/compare_func_sizes.py \
		--only-tcc-larger \
		--min-percent-over $(SQLITE_REPORT_MIN_PERCENT) \
		--show-percent \
		--limit $(SQLITE_REPORT_LIMIT) \
		$(STAGE0) $(STAGE1)

report-sqlite-func-sizes: $(STAGE0) $(STAGE1)
	@set -eu; \
	sqlite_dir="$(SQLITE_SMOKE_DIR)"; \
	stage0_compiler="$(abspath $(STAGE0))"; \
	stage1_compiler="$(abspath $(STAGE1))"; \
	if [ ! -f "$$sqlite_dir/main.c" ] || [ ! -f "$$sqlite_dir/sqlite3.c" ]; then \
		echo "SQLite report sources not found under: $$sqlite_dir"; \
		echo "Override SQLITE_SMOKE_DIR to point at a sqlite amalgamation tree."; \
		exit 1; \
	fi; \
	workdir="$$(mktemp -d /tmp/tcc-sqlite-report.XXXXXX)"; \
	trap 'rm -rf "$$workdir"' EXIT INT TERM HUP; \
	stage0_bin="$$workdir/sqlite_stage0"; \
	stage1_bin="$$workdir/sqlite_stage1"; \
	echo "SQLite function-size report (stage0 vs stage1):"; \
	echo "  sources: $$sqlite_dir"; \
	(cd "$$sqlite_dir" && "$$stage0_compiler" main.c sqlite3.c $(SQLITE_SMOKE_LIBS) -o "$$stage0_bin"); \
	(cd "$$sqlite_dir" && "$$stage1_compiler" main.c sqlite3.c $(SQLITE_SMOKE_LIBS) -o "$$stage1_bin"); \
	python3 tools/compare_func_sizes.py \
		--only-tcc-larger \
		--min-percent-over $(SQLITE_REPORT_MIN_PERCENT) \
		--show-percent \
		--limit $(SQLITE_REPORT_LIMIT) \
		"$$stage0_bin" "$$stage1_bin"

report-sqlite-stage-times: $(STAGE0) $(STAGE1)
	@set -eu; \
	sqlite_dir="$(SQLITE_SMOKE_DIR)"; \
	stage0_compiler="$(abspath $(STAGE0))"; \
	stage1_compiler="$(abspath $(STAGE1))"; \
	if [ ! -f "$$sqlite_dir/main.c" ] || [ ! -f "$$sqlite_dir/sqlite3.c" ]; then \
		echo "SQLite report sources not found under: $$sqlite_dir"; \
		echo "Override SQLITE_SMOKE_DIR to point at a sqlite amalgamation tree."; \
		exit 1; \
	fi; \
	workdir="$$(mktemp -d /tmp/tcc-sqlite-times.XXXXXX)"; \
	trap 'rm -rf "$$workdir"' EXIT INT TERM HUP; \
	echo "SQLite stage timing report:"; \
	echo "  sources: $$sqlite_dir"; \
	echo "  workdir: $$workdir"; \
	echo "  stage0:"; \
	(cd "$$sqlite_dir" && /usr/bin/time -p "$$stage0_compiler" main.c sqlite3.c $(SQLITE_SMOKE_LIBS) -o "$$workdir/sqlite_stage0") 2>&1; \
	echo "  stage1:"; \
	(cd "$$sqlite_dir" && /usr/bin/time -p "$$stage1_compiler" main.c sqlite3.c $(SQLITE_SMOKE_LIBS) -o "$$workdir/sqlite_stage1") 2>&1

report-release-metrics: report-selfhost-func-sizes report-sqlite-func-sizes report-sqlite-stage-times

mips: $(STAGE0) | $(TMP_DIR)
	@while IFS= read -r entry; do \
		case "$$entry" in ''|\#*) continue ;; esac; \
		t=$${entry%%:*}; base=$${t%.c}; \
		$(STAGE0) -target=mips $(TEST_DIR)/$$t > $(TMP_DIR)/$$base.mips.s; \
		echo "Generated $$base.mips.s"; \
	done < $(TEST_DIR)/manifest.txt

# ---------------------------------------------------------------------------
# Installation (installs stage1 as the production binary)
# ---------------------------------------------------------------------------

PREFIX    ?= /usr/local
BINDIR     = $(PREFIX)/bin
INCLUDEDIR = $(PREFIX)/include/tcc

install: $(STAGE1)
	@echo "Installing tcc (stage1 self-hosted) to $(BINDIR)/tcc"
	@mkdir -p $(BINDIR)
	install -m 755 $(STAGE1) $(BINDIR)/tcc
	@echo "Installing headers to $(INCLUDEDIR)"
	@mkdir -p $(INCLUDEDIR)/sys
	cp $(SRC_DIR)/include/*.h $(INCLUDEDIR)/
	cp $(SRC_DIR)/include/sys/*.h $(INCLUDEDIR)/sys/
	@echo "Done. Run: tcc --version"

uninstall:
	@echo "Removing $(BINDIR)/tcc"
	@rm -f $(BINDIR)/tcc
	@echo "Removing $(INCLUDEDIR)"
	@rm -rf $(INCLUDEDIR)
	@echo "Done."

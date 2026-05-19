CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

SRC_DIR = cc
TEST_DIR = tests
BUILD_DIR = build
TMP_DIR = $(BUILD_DIR)/tmp

TARGET = $(BUILD_DIR)/tcc

SRCS = \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/lexer.c \
	$(SRC_DIR)/preprocess.c \
	$(SRC_DIR)/parser.c \
	$(SRC_DIR)/ast.c \
	$(SRC_DIR)/ir.c \
	$(SRC_DIR)/emit.c \
	$(SRC_DIR)/helpers.c \
	$(SRC_DIR)/codegen/x86.c \
	$(SRC_DIR)/codegen/x64.c \
	$(SRC_DIR)/codegen/arm64.c \
	$(SRC_DIR)/codegen/mips.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_MANIFEST = $(TEST_DIR)/manifest.txt
TESTS_ALL = $(shell egrep -v '^\#' $(TEST_MANIFEST) | sort)

# Optional: filter to a single category. Use: make test CATEGORY=abi
ifdef CATEGORY
TESTS = $(filter $(CATEGORY)/%,$(TESTS_ALL))
else
TESTS = $(TESTS_ALL)
endif

# Test runner configuration. Override TEST_CC to run the suite with a
# different compiler, e.g. build/tcc_stage1.
TEST_CC ?= $(TARGET)

# Pick the default test target from the host CPU architecture.
# This is deliberately overrideable, e.g.:
#   make test TEST_TARGET=arm64
#   make test TEST_TARGET=x64
HOST_ARCH := $(shell uname -m 2>/dev/null || arch 2>/dev/null || echo unknown)

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
$(error Unsupported host architecture '$(HOST_ARCH)'; set TEST_TARGET manually, e.g. make test TEST_TARGET=arm64)
endif

TEST_AS ?= clang

.PHONY: all clean clobber test test-stage1 test-abi test-arrays test-chars test-control test-core test-globals test-loops test-lvalue test-opt test-pointers test-preproc test-strings test-structs test-types test-stage1-category smoketest install uninstall test-preprocess test-ir-strict test-cfg dump-ast arm64 x86 mips

all: $(TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(TMP_DIR): | $(BUILD_DIR)
	@mkdir -p $(TMP_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(SRC_DIR)/codegen -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f out out.s out.o *.asm test-results.txt test.o test.s

clobber: clean
	rm -f *.dSYM

test: $(TARGET) | $(TMP_DIR)
	@fail=0; \
	tests=0; \
	works=0; \
	last=0; \
	class=""; \
	echo "Running tests:"; \
	echo "  compiler: $(TEST_CC)"; \
	echo "  target:   $(TEST_TARGET)"; \
	for entry in $(TESTS); do \
		t=$${entry%%:*}; \
		expected=$${entry##*:}; \
		base=$${t%.c}; \
		newclass=$${t%%/*}; \
		if [ "$$newclass" != "$$class" ]; then \
			[ $$last -eq 1 ] && echo; \
			class=$$newclass; \
			printf "%s: " "$$class"; \
			last=0; \
		fi; \
		tests=$$((tests + 1)); \
		if ! $(TEST_CC) -S -target=$(TEST_TARGET) $(TEST_DIR)/$$t > $(TMP_DIR)/out.s; then \
			[ $$last -eq 1 ] && echo; \
			echo "FAIL $$base (compiler error)"; \
			fail=$$((fail + 1)); \
			last=0; \
			continue; \
		fi; \
		if ! $(TEST_AS) $(TMP_DIR)/out.s -o $(TMP_DIR)/out; then \
			[ $$last -eq 1 ] && echo; \
			echo "FAIL $$base (assembler/linker error)"; \
			fail=$$((fail + 1)); \
			last=0; \
			continue; \
		fi; \
		$(TMP_DIR)/out 2> $(TMP_DIR)/out.err; actual=$$?; \
		if [ "$$actual" = "$$expected" ]; then \
			printf "."; \
			works=$$((works + 1)); \
			last=1; \
		else \
			[ $$last -eq 1 ] && echo; \
			echo "FAIL $tt $$base (expected=$$expected actual=$$actual)"; \
			fail=$$((fail + 1)); \
			last=0; \
		fi; \
	done; \
	[ $$last -eq 1 ] && echo; \
	echo "------------------------------"; \
	echo "$$tests/$$tests Completed"; \
	echo "$$fail Failed"; \
	echo "------------------------------"; \
	exit $$fail


# ---------------------------------------------------------------------------
# Per-category test targets: make test-abi, make test-arrays, etc.
# Also supports: make test CATEGORY=abi
# ---------------------------------------------------------------------------
TEST_CATEGORIES = abi arrays chars control core globals loops lvalue opt pointers preproc strings structs types

.PHONY: test-abi
test-abi: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=abi

.PHONY: test-arrays
test-arrays: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=arrays

.PHONY: test-chars
test-chars: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=chars

.PHONY: test-control
test-control: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=control

.PHONY: test-core
test-core: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=core

.PHONY: test-globals
test-globals: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=globals

.PHONY: test-loops
test-loops: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=loops

.PHONY: test-lvalue
test-lvalue: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=lvalue

.PHONY: test-opt
test-opt: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=opt

.PHONY: test-pointers
test-pointers: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=pointers

.PHONY: test-preproc
test-preproc: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=preproc

.PHONY: test-strings
test-strings: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=strings

.PHONY: test-structs
test-structs: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=structs

.PHONY: test-types
test-types: $(TARGET) | $(TMP_DIR)
	$(MAKE) test CATEGORY=types

.PHONY: test-stage1-category
test-stage1-category: $(STAGE1) | $(TMP_DIR)
	$(MAKE) test CATEGORY=$(CATEGORY) TEST_CC=$(STAGE1)

# Run the normal manifest using the self-hosted stage1 compiler.
# This target only requires build/tcc_stage1 to exist/link; it does not run
# the stage1 smoke target first, so it can be used while debugging smoke.
test-stage1: TEST_CC = $(STAGE1)
test-stage1: $(STAGE1) test

dump-ast: $(TARGET)
	$(TARGET) -dump-ast $(TEST_DIR)/core/test008.c

arm64: $(TARGET) | $(TMP_DIR)
	@for entry in $(TESTS); do \
		t=$${entry%%:*}; \
		base=$${t%.c}; \
		echo "Generating/running $$base"; \
		$(TARGET) -S -target=arm64 $(TEST_DIR)/$$t > $(TMP_DIR)/out.s; \
		clang $(TMP_DIR)/out.s -o $(TMP_DIR)/out; \
		$(TMP_DIR)/out; echo "exit code: $$?"; \
	done

x86: $(TARGET)
	@for entry in $(TESTS); do \
		t=$${entry%%:*}; \
		base=$${t%.c}; \
		$(TARGET) -target=x86 $(TEST_DIR)/$$t > $$base.asm; \
		echo "Generated $$base.asm"; \
	done


test-preprocess: $(TARGET) | $(TMP_DIR)
	@fail=0; \
	echo "Running preprocess tests:"; \
	for t in $(TEST_DIR)/preproc/*.expected; do \
		base=$${t%.expected}; \
		if ! $(TARGET) -target=x86 -E $$base.c > $(TMP_DIR)/pre.out; then \
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


mips: $(TARGET) | $(TMP_DIR)
	@for entry in $(TESTS); do \
		t=$${entry%%:*}; \
		base=$${t%.c}; \
		$(TARGET) -target=mips $(TEST_DIR)/$$t > $(TMP_DIR)/$$base.mips.s; \
		echo "Generated $$base.mips.s"; \
	done


test-ir-strict: $(TARGET) | $(TMP_DIR)
	@fail=0; \
	tests=0; \
	echo "Running IR strict generation tests:"; \
	for entry in $(TESTS); do \
		t=$${entry%%:*}; \
		base=$${t%.c}; \
		tests=$$((tests + 1)); \
		if $(TARGET) -S -fir-strict -target=arm64 $(TEST_DIR)/$$t > $(TMP_DIR)/out.s 2> $(TMP_DIR)/ir.err; then \
			printf "."; \
		else \
			if grep -q "struct" $(TMP_DIR)/ir.err; then \
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
	done; \
	echo; \
	echo "------------------------------"; \
	echo "$$tests/$$tests Checked"; \
	echo "$$fail Failed"; \
	echo "------------------------------"; \
	exit $$fail


test-cfg: $(TARGET)
	@echo "Dumping CFG smoke test:"
	@$(TARGET) -dump-cfg tests/control/test001.c | head -20

# ---------------------------------------------------------------------------
# Self-hosting stage1 build
# Uses build/tcc -target=arm64 -c to compile each source to a .o,
# then links with clang to produce build/tcc_stage1.
# ---------------------------------------------------------------------------

SELFHOST_TARGET ?= arm64
SELFHOST_DIR    = $(BUILD_DIR)/stage1
STAGE1          = $(BUILD_DIR)/tcc_stage1

SELFHOST_OBJS = $(patsubst $(SRC_DIR)/%.c,$(SELFHOST_DIR)/%.o,$(SRCS))

.PHONY: stage1
stage1: $(STAGE1)
	@echo

$(STAGE1): $(SELFHOST_OBJS) | $(SELFHOST_DIR)
	clang $^ -o $@

$(SELFHOST_DIR)/%.o: $(SRC_DIR)/%.c $(TARGET) | $(SELFHOST_DIR)
	@mkdir -p $(dir $@)
	$(TARGET) -target=$(SELFHOST_TARGET) -c $< -o $@

$(SELFHOST_DIR): | $(BUILD_DIR)
	@mkdir -p $@

.PHONY: smoketest
smoketest: $(STAGE1) | $(SELFHOST_DIR)
	@echo "Smoke test:"
	@echo "  $(STAGE1) -S -target=$(SELFHOST_TARGET) $(SRC_DIR)/lexer.c -o $(SELFHOST_DIR)/smoke.s"
	@$(STAGE1) -S -target=$(SELFHOST_TARGET) $(SRC_DIR)/lexer.c -o $(SELFHOST_DIR)/smoke.s
	@echo "Stage1 smoke test OK  ($(SELFHOST_DIR)/smoke.s)"

# ---------------------------------------------------------------------------
# Installation  (installs the self-hosted stage1 binary, not the bootstrap)
# ---------------------------------------------------------------------------

PREFIX      ?= /usr/local
BINDIR       = $(PREFIX)/bin
INCLUDEDIR   = $(PREFIX)/include/tcc

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


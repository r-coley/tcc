#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "tcc.h"
#include "ast.h"
#include "codegen/codegen.h"
#include "emit.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "preprocess.h"

extern Codegen x86_codegen;
extern Codegen x64_codegen;
extern Codegen arm64_codegen;
extern Codegen mips_codegen;

typedef struct {
	int optimize_level;
	int debug;
	int emit_asm_only;
	int compile_only;   /* -c: produce .o via assembler */
	int preprocess_only;
	const char *output_file;
} Options;

typedef struct {
	Codegen *codegen;
	PreprocessTarget pp_target;
} TargetSelection;

static TargetSelection 
host_target_selection(void)
{
	TargetSelection sel;
#if defined(__aarch64__) || defined(__arm64__)
	sel.codegen = &arm64_codegen;
	sel.pp_target = PP_TARGET_ARM64;
#elif defined(__x86_64__) || defined(_M_X64)
	sel.codegen = &x64_codegen;
	sel.pp_target = PP_TARGET_X64;
#elif defined(__i386__) || defined(_M_IX86)
	sel.codegen = &x86_codegen;
	sel.pp_target = PP_TARGET_X86;
#elif defined(__mips__)
	sel.codegen = &mips_codegen;
	sel.pp_target = PP_TARGET_MIPS;
#else
	sel.codegen = &x86_codegen;
	sel.pp_target = PP_TARGET_X86;
#endif
	return sel;
}


static char *
default_obj_output_name(const char *input)
{
	const char *slash = strrchr(input, '/');
	const char *base = slash ? slash + 1 : input;
	const char *dot = strrchr(base, '.');
	size_t stem_len = dot ? (size_t)(dot - input) : strlen(input);

	char *out = xmalloc(stem_len + 3);
	memcpy(out, input, stem_len);
	out[stem_len] = '\0';
	strcat(out, ".o");
	return out;
}

static const char *
pp_target_arch(PreprocessTarget t)
{
	switch (t) {
	case PP_TARGET_ARM64:
		return "arm64";
	case PP_TARGET_X64:
		return "x86_64";
	case PP_TARGET_X86:
		return "i386";
	case PP_TARGET_MIPS:
		return "mips";
	default:
		return "arm64";
	}
}

static char *
make_temp_asm(void)
{
	char *tmp = xmalloc(64);	snprintf(tmp, 64, "/tmp/tcc_%d.s", (int)getpid());
	return tmp;
}

static char *
read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) {
		perror(path);
		exit(1);
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		perror("fseek");
		fclose(file);
		exit(1);
	}

	long size = ftell(file);
	if (size < 0) {
		perror("ftell");
		fclose(file);
		exit(1);
	}

	rewind(file);

	char *buffer = xcalloc(1, (size_t)size + 1);
	size_t nread = fread(buffer, 1, (size_t)size, file);
	if (nread != (size_t)size) {
		tcc_error("Failed to read complete file\n");
		xfree(buffer);
		fclose(file);
	}

	fclose(file);
	return buffer;
}

static void 
usage(const char *argv0)
{
	fprintf(stderr,
	        "Usage: %s [options] file.c\n"
	        "Options:\n"
	        "  -target=x86|x64|x86_64|arm64|mips   Select output target (default: host)\n"
	        "  -asm=nasm|gas-intel|gas-att         Select assembly dialect (x86)\n"
	        "  -dump-ast           Print AST instead of assembly\n"
	        "  -dump-ir            Print simple IR instead of assembly\n"
	        "  -dump-ir-lowered    Alias for -dump-ir\n"
	        "  -fno-ast-fallback   Error if IR cannot codegen the input\n"
	        "  -static             Produce output for a static binary (no GOT)\n"
	        "  -fir-struct-fallback-only  Error on non-struct AST fallback\n"
	        "  -fir-strict         Alias for -fir-struct-fallback-only\n"
	        "  -ftrace-codegen    Print IR/AST codegen path to stderr\n"
	        "  -o <file>           Write output to file\n"
	        "  -c                  Compile to object file (.o) via system assembler\n"
	        "  -S                  Emit assembly to <input>.s unless -o is used\n"
	        "  -E                  Preprocess only\n"
	        "  -V                  Print compiler version\n"
	        "  -g                  Emit DWARF line info (.file/.loc directives) for debugging\n"
	        "  -O, -O0, -O1, -O2   Accept/record optimization level\n"
	        "  -std=<standard>     Accepted but ignored for now\n",
	        argv0);
}

void
cleanup()
{
	/*** Cleanup malloc'd space ***/
	/* xfree(if_stack); */
	/* xfree(expanded_arg_text); */
	/* xfree(pieces); */
}

int
main(int argc, char **argv)
{
	atexit(cleanup);
	TargetSelection default_target = host_target_selection();
	Codegen *cg = default_target.codegen;
	PreprocessTarget pp_target = default_target.pp_target;
	const char *input = NULL;
	int dump = 0;
	int dump_ir = 0;
	int dump_cfg = 0;
	int require_ir = 0;
	int trace_codegen = 0;
	int strict_ir_except_structs = 0;
	Options opts = {0};
	AsmDialect requested_dialect = ASM_DIALECT_DEFAULT;
	LinkModel requested_link_model = LINK_DYNAMIC;

	for (int i = 1; i < argc; i++) {
		if (STRCMP(argv[i], "-debug") == 0) {
			fprintf(stderr,"Debugging enabled\n");
			Debug_lvl++;
		} else if (STRCMP(argv[i], "-target=arm64") == 0) {
			cg = &arm64_codegen;
			pp_target = PP_TARGET_ARM64;
		} else if (STRCMP(argv[i], "-target=mips") == 0) {
			cg = &mips_codegen;
			pp_target = PP_TARGET_MIPS;
		} else if (STRCMP(argv[i], "-target=x64") == 0 || STRCMP(argv[i], "-target=x86_64") == 0) {
			cg = &x64_codegen;
			pp_target = PP_TARGET_X64;
		} else if (STRCMP(argv[i], "-target=x86") == 0) {
			cg = &x86_codegen;
			pp_target = PP_TARGET_X86;
		} else if (STRCMP(argv[i], "-asm=nasm") == 0) {
			requested_dialect = ASM_DIALECT_NASM;
		} else if (STRCMP(argv[i], "-asm=gas-intel") == 0 || STRCMP(argv[i], "-asm=intel") == 0) {
			requested_dialect = ASM_DIALECT_GAS_INTEL;
		} else if (STRCMP(argv[i], "-asm=gas-att") == 0 || STRCMP(argv[i], "-asm=att") == 0) {
			requested_dialect = ASM_DIALECT_GAS_ATT;
		} else if (STRCMP(argv[i], "-dump-ast") == 0) {
			dump = 1;
		} else if (STRCMP(argv[i], "-dump-ir") == 0 || STRCMP(argv[i], "-dump-ir-lowered") == 0) {
			dump_ir = 1;
		} else if (STRCMP(argv[i], "-dump-cfg") == 0) {
			dump_cfg = 1;
		} else if (STRCMP(argv[i], "-fno-ast-fallback") == 0) {
			require_ir = 1;
		} else if (STRCMP(argv[i], "-static") == 0) {
			requested_link_model = LINK_STATIC;
		} else if (STRCMP(argv[i], "-ftrace-codegen") == 0) {
			trace_codegen = 1;
		} else if (STRCMP(argv[i], "-fir-struct-fallback-only") == 0 || STRCMP(argv[i], "-fir-strict") == 0) {
			strict_ir_except_structs = 1;
		} else if (STRCMP(argv[i], "-c") == 0) {
			opts.compile_only = 1;
		} else if (STRCMP(argv[i], "-g") == 0) {
			opts.debug = 1;
		} else if (STRCMP(argv[i], "-S") == 0) {
			opts.emit_asm_only = 1;
		} else if (STRCMP(argv[i], "-E") == 0) {
			opts.preprocess_only = 1;
		} else if (STRCMP(argv[i], "-dD") == 0) {
			preprocess_set_line_markers(1);
		} else if (STRCMP(argv[i], "-V") == 0) {
			printf("tcc version %s\n", TCC_VERSION);
			return 0;
		} else if (strncmp(argv[i], "-O", 2) == 0) {
			if (argv[i][2] == '\0')
				opts.optimize_level = 1;
			else if (argv[i][2] >= '0' && argv[i][2] <= '9' && argv[i][3] == '\0')
				opts.optimize_level = argv[i][2] - '0';
			else {
				tcc_warn("Unsupported optimization option: %s\n", argv[i]);
				return 1;
			}
		} else if (strncmp(argv[i], "-std=", 5) == 0) {
			/* accepted but ignored for now */
		} else if (STRCMP(argv[i], "-o") == 0) {
			if (i + 1 >= argc) {
				tcc_error("-o requires a filename\n");
				return 1;
			}
			opts.output_file = argv[++i];
		} else if (argv[i][0] == '-') {
			tcc_error("Unknown option: %s\n", argv[i]);
			usage(argv[0]);
			return 1;
		} else {
			input = argv[i];
		}
	}

	if (!input) {
		usage(argv[0]);
		return 1;
	}

	char *generated_output_file = NULL;
	char *temp_asm_file = NULL;

	const char *compile_only_obj = NULL;  /* user-specified .o name via -o */
	const char *link_output = NULL;       /* executable name for full compile+link */
	if (opts.compile_only) {
		/* -c: generate asm to temp file, then assemble to .o */
		compile_only_obj = opts.output_file;  /* may be NULL or user's -o value */
		temp_asm_file = make_temp_asm();
		opts.output_file = temp_asm_file;
	} else if (!opts.emit_asm_only && !dump && !dump_ir && !dump_cfg && !opts.preprocess_only) {
		/* Full compile+link: generate asm to temp file, then assemble+link */
		link_output = opts.output_file ? opts.output_file : "a.out";
		temp_asm_file = make_temp_asm();
		opts.output_file = temp_asm_file;
	}
	/* -S without -o: output_file stays NULL -> assembly written to stdout */

	if (requested_dialect == ASM_DIALECT_GAS_ATT) {
		tcc_error("-asm=gas-att not implemented yet\n");
		return 1;
	}
	codegen_set_asm_dialect(requested_dialect);
	codegen_set_link_model(requested_link_model);

	/* Locate the compiler's include directory.
	 * Priority: TCC_INCLUDE_DIR env var > argv[0]-relative > installed default. */
	{
		const char *env_inc = getenv("TCC_INCLUDE_DIR");
		if (env_inc) {
			preprocess_set_include_dir(env_inc);
		} else {
			/* Try <dir-of-argv0>/../include/tcc (for installed builds) */
			char inc_path[512];
			const char *slash = strrchr(argv[0], '/');
			if (slash) {
				int dirlen = (int)(slash - argv[0]);
				if (dirlen > 500) dirlen = 500;
				int i = 0;
				while (i < dirlen) { inc_path[i] = argv[0][i]; i++; }
				const char *suffix = "/../include/tcc";
				int j = 0;
				while (suffix[j] && i < 511) inc_path[i++] = suffix[j++];
				inc_path[i] = '\0';
				preprocess_set_include_dir(inc_path);
			} else {
				preprocess_set_include_dir("/usr/local/include/tcc");
			}
		}
	}
	/* Enable debug info if -g was specified */
	if (opts.debug && cg == &arm64_codegen)
		arm64_set_debug(1);

	preprocess_configure(pp_target);
	/*
	 * During normal compilation the parser sees a single flattened
	 * preprocessed buffer.  Preserve include/source provenance in that
	 * buffer with #line markers so lexer token locations, and therefore
	 * fatal_token() diagnostics, report the original file/line.
	 *
	 * For -E, keep the historical clean preprocessor output unless the
	 * user explicitly requested markers with -dD.
	 */
	if (!opts.preprocess_only)
		preprocess_set_line_markers(1);

	char *source = read_file(input);
	char *expanded = preprocess(input,source);

	if (opts.preprocess_only) {
		fputs(expanded, stdout);
		xfree(expanded);
		xfree(source);
		return 0;
	}

	Node *program = parse_program(input,expanded);
	program = fold_constants(program);
	program = eliminate_dead_code(program);

	FILE *old_stdout = NULL;
	int saved_stdout_fd = -1;

	if (opts.output_file) {
		fflush(stdout);
		saved_stdout_fd = dup(1);
		old_stdout = fopen(opts.output_file, "w");
		if (!old_stdout) {
			perror(opts.output_file);
			free_ast(program);
			xfree(expanded);
			xfree(source);
			return 1;
		}
		dup2(fileno(old_stdout), 1);
	}

	IRProgram *ir = ir_build(program);
	ir_optimize(ir, opts.optimize_level);

	if (dump) {
		dump_ast(program, 0);
	} else if (dump_ir) {
		ir_dump(ir);
	} else if (dump_cfg) {
		ir_dump_cfg(ir);
	} else if (ir_can_codegen(ir)) {
		if (opts.debug || trace_codegen)
			fprintf(stderr, "[tcc] codegen=IR\n");
		ir_emit_program(ir, cg);
	} else if (require_ir || (strict_ir_except_structs &&
	                          (!ir_has_unsupported(ir) || strstr(ir_unsupported_reason(ir), "struct") == NULL))) {
		fprintf(stderr, "IR lowering/codegen incomplete");
		if (ir_has_unsupported(ir))
			fprintf(stderr, ": %s", ir_unsupported_reason(ir));
		fprintf(stderr, "\n");
		ir_free(ir);
		free_ast(program);
		xfree(expanded);
		xfree(source);
		return 1;
	} else {
		if (opts.debug || trace_codegen) {
			fprintf(stderr, "[tcc] codegen=AST fallback");
			if (ir_has_unsupported(ir))
				fprintf(stderr, ": %s", ir_unsupported_reason(ir));
			fprintf(stderr, "\n");
		}
		emit_program(program, cg);
	}

	ir_free(ir);

	if (opts.output_file) {
		fflush(stdout);
		fclose(old_stdout);
		dup2(saved_stdout_fd, 1);
		close(saved_stdout_fd);
	}

	free_ast(program);
	xfree(expanded);
	xfree(source);
	xfree(generated_output_file);

	if (opts.compile_only && temp_asm_file) {
		/* Assemble the temp .s file to produce the .o */
		const char *arch = pp_target_arch(pp_target);
		/* Determine .o output name */
		char *obj_name = NULL;
		if (compile_only_obj) {
			obj_name = (char *)compile_only_obj;
		} else {
			obj_name = default_obj_output_name(input);
		}

		char cmd[1024];
		(void)arch;  /* arch used by 'as' on macOS; cc handles it automatically */
		snprintf(cmd, sizeof(cmd), "cc -c %s -o %s",
		         temp_asm_file, obj_name);
		int ret = system(cmd);

		if (!compile_only_obj)
			xfree(obj_name);
		remove(temp_asm_file);
		xfree(temp_asm_file);

		if (ret != 0) {
			tcc_error("Assembler failed\n");
			return 1;
		}
	} else if (link_output && temp_asm_file) {
		/* Assemble + link the temp .s file to produce an executable */
		char cmd[1024];
		snprintf(cmd, sizeof(cmd), "cc %s -o %s",
		         temp_asm_file, link_output);
		int ret = system(cmd);

		remove(temp_asm_file);
		xfree(temp_asm_file);

		if (ret != 0) {
			tcc_error("Link failed\n");
			return 1;
		}
	}

	return 0;
}

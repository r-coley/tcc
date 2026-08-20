#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tcc.h"
#include "ast.h"
#include "codegen/codegen.h"
#include "codegen/arm64_peephole.h"
#include "emit.h"
#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include "preprocess.h"

#undef xmalloc
#undef xcalloc
#undef xrealloc
#undef xfree
#define xmalloc(size)        xmalloc_impl((size), "cc/main.c", __LINE__, "main")
#define xcalloc(count, size) xcalloc_impl((count), (size), "cc/main.c", __LINE__, "main")
#define xrealloc(ptr, size)  xrealloc_impl((ptr), (size), "cc/main.c", __LINE__, "main")
#define xfree(ptr)           xfree_impl((ptr), "cc/main.c", __LINE__, "main")

extern Codegen x86_codegen;
extern Codegen x64_codegen;
extern Codegen arm64_codegen;
extern Codegen mips_codegen;
extern Codegen m68k_codegen;

typedef struct {
	int optimize_level;
	int debug;
	int emit_asm_only;
	int compile_only;   /* -c: produce .o via assembler */
	int preprocess_only;
	int emit_dwarf_scaffold;
	int bootstrap_includes;
	const char *output_file;
} Options;

static int g_trace_phases = 0;
static int g_time_report = 0;
static void trace_unsupported_functions(IRProgram *ir);

static void
trace_main_boot(const char *msg)
{
	const char *trace = getenv("TCC_TRACE_MAIN_BOOT");
	if (!trace || !trace[0])
		return;
	fprintf(stderr, "MAINBOOT %s\n", msg);
	fflush(stderr);
}

#if defined(__APPLE__)
static void
add_macos_sdk_include_dir(void)
{
	static const char *fallbacks[] = {
		"/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include",
		"/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include"
	};
	const char *sdkroot = getenv("SDKROOT");
	int i;

	if (sdkroot && sdkroot[0]) {
		char sdk_inc[512];
		int n = snprintf(sdk_inc, sizeof(sdk_inc), "%s/usr/include", sdkroot);
		if (n > 0 && n < (int)sizeof(sdk_inc) && access(sdk_inc, 0) == 0) {
			preprocess_set_include_dir(sdk_inc);
			return;
		}
	}

	for (i = 0; i < (int)(sizeof(fallbacks) / sizeof(fallbacks[0])); i++) {
		if (access(fallbacks[i], 0) == 0) {
			preprocess_set_include_dir(fallbacks[i]);
			return;
		}
	}
}
#endif

static void
trace_unsupported_functions(IRProgram *ir)
{
	int count;

	if (!ir)
		return;
	count = ir_unsupported_function_count(ir);
	if (count <= 0)
		return;
	fprintf(stderr, " functions=");
	for (int i = 0; i < count; i++) {
		const char *name = ir_unsupported_function_name(ir, i);
		const char *reason = ir_unsupported_function_reason(ir, i);
		fprintf(stderr, "%s%s", i == 0 ? "" : ",", name ? name : "<anon>");
		if (reason && reason[0])
			fprintf(stderr, "{%s}", reason);
	}
}

typedef struct {
	double read_input;
	double preprocess;
	double parse;
	double ast_opt;
	double ir_build;
	double ir_optimize;
	double emit;
	double emit_hybrid_strings;
	double emit_hybrid_ir;
	double emit_hybrid_ast;
	double emit_ir_collect_strings;
	double emit_ir_setup;
	double emit_ir_body;
	double emit_ir_debug_finish;
	double emit_ir_named_lookup;
	double emit_ir_named_stream;
	double assemble;
	double link;
	double dsymutil;
	double total;
	PreprocessProfile preprocess_profile;
	ParserProfile parser_profile;
} TimeReport;

LangStandard tcc_lang_standard = LANG_C23;
int tcc_iso_diagnostics = 0;

static int
parse_lang_standard(const char *value, int *out)
{
	if (STRCMP(value, "c89") == 0 ||
	    STRCMP(value, "c90") == 0 ||
	    STRCMP(value, "iso9899:1990") == 0) {
		*out = STRCMP(value, "c89") == 0 ? LANG_C89 : LANG_C90;
		return 1;
	}
	if (STRCMP(value, "c99") == 0 || STRCMP(value, "iso9899:1999") == 0) {
		*out = LANG_C99;
		return 1;
	}
	if (STRCMP(value, "c11") == 0 || STRCMP(value, "iso9899:2011") == 0) {
		*out = LANG_C11;
		return 1;
	}
	if (STRCMP(value, "c17") == 0 ||
	    STRCMP(value, "c18") == 0 ||
	    STRCMP(value, "iso9899:2017") == 0 ||
	    STRCMP(value, "iso9899:2018") == 0) {
		*out = LANG_C17;
		return 1;
	}
	if (STRCMP(value, "c23") == 0 ||
	    STRCMP(value, "c2x") == 0 ||
	    STRCMP(value, "iso9899:2024") == 0) {
		*out = LANG_C23;
		return 1;
	}
	return 0;
}

static void
time_report_add(TimeReport *dst, const TimeReport *src)
{
	int i;
	if (!dst || !src)
		return;

	dst->read_input += src->read_input;
	dst->preprocess += src->preprocess;
	dst->parse += src->parse;
	dst->ast_opt += src->ast_opt;
	dst->ir_build += src->ir_build;
	dst->ir_optimize += src->ir_optimize;
	dst->emit += src->emit;
	dst->emit_hybrid_strings += src->emit_hybrid_strings;
	dst->emit_hybrid_ir += src->emit_hybrid_ir;
	dst->emit_hybrid_ast += src->emit_hybrid_ast;
	dst->emit_ir_collect_strings += src->emit_ir_collect_strings;
	dst->emit_ir_setup += src->emit_ir_setup;
	dst->emit_ir_body += src->emit_ir_body;
	dst->emit_ir_debug_finish += src->emit_ir_debug_finish;
	dst->emit_ir_named_lookup += src->emit_ir_named_lookup;
	dst->emit_ir_named_stream += src->emit_ir_named_stream;
	dst->assemble += src->assemble;
	dst->link += src->link;
	dst->dsymutil += src->dsymutil;
	dst->total += src->total;
	dst->preprocess_profile.trigraph_time += src->preprocess_profile.trigraph_time;
	dst->preprocess_profile.child_trigraph_time += src->preprocess_profile.child_trigraph_time;
	dst->preprocess_profile.join_time += src->preprocess_profile.join_time;
	dst->preprocess_profile.child_join_time += src->preprocess_profile.child_join_time;
	dst->preprocess_profile.strip_comments_time += src->preprocess_profile.strip_comments_time;
	dst->preprocess_profile.child_strip_comments_time += src->preprocess_profile.child_strip_comments_time;
	dst->preprocess_profile.macro_expand_time += src->preprocess_profile.macro_expand_time;
	dst->preprocess_profile.child_macro_expand_time += src->preprocess_profile.child_macro_expand_time;
	dst->preprocess_profile.macro_tokenize_time += src->preprocess_profile.macro_tokenize_time;
	dst->preprocess_profile.child_macro_tokenize_time += src->preprocess_profile.child_macro_tokenize_time;
	dst->preprocess_profile.object_macro_time += src->preprocess_profile.object_macro_time;
	dst->preprocess_profile.child_object_macro_time += src->preprocess_profile.child_object_macro_time;
	dst->preprocess_profile.function_macro_time += src->preprocess_profile.function_macro_time;
	dst->preprocess_profile.child_function_macro_time += src->preprocess_profile.child_function_macro_time;
	dst->preprocess_profile.function_macro_arg_collect_time += src->preprocess_profile.function_macro_arg_collect_time;
	dst->preprocess_profile.child_function_macro_arg_collect_time += src->preprocess_profile.child_function_macro_arg_collect_time;
	dst->preprocess_profile.function_macro_arg_expand_time += src->preprocess_profile.function_macro_arg_expand_time;
	dst->preprocess_profile.child_function_macro_arg_expand_time += src->preprocess_profile.child_function_macro_arg_expand_time;
	dst->preprocess_profile.function_macro_build_time += src->preprocess_profile.function_macro_build_time;
	dst->preprocess_profile.child_function_macro_build_time += src->preprocess_profile.child_function_macro_build_time;
	dst->preprocess_profile.function_macro_tail_time += src->preprocess_profile.function_macro_tail_time;
	dst->preprocess_profile.child_function_macro_tail_time += src->preprocess_profile.child_function_macro_tail_time;
	dst->preprocess_profile.macro_rescan_time += src->preprocess_profile.macro_rescan_time;
	dst->preprocess_profile.child_macro_rescan_time += src->preprocess_profile.child_macro_rescan_time;
	dst->preprocess_profile.directive_time += src->preprocess_profile.directive_time;
	dst->preprocess_profile.child_directive_time += src->preprocess_profile.child_directive_time;
	dst->preprocess_profile.directive_normalize_time += src->preprocess_profile.directive_normalize_time;
	dst->preprocess_profile.conditional_time += src->preprocess_profile.conditional_time;
	dst->preprocess_profile.child_conditional_time += src->preprocess_profile.child_conditional_time;
	dst->preprocess_profile.define_time += src->preprocess_profile.define_time;
	dst->preprocess_profile.child_define_time += src->preprocess_profile.child_define_time;
	dst->preprocess_profile.undef_time += src->preprocess_profile.undef_time;
	dst->preprocess_profile.include_time += src->preprocess_profile.include_time;
	dst->preprocess_profile.child_include_time += src->preprocess_profile.child_include_time;
	dst->preprocess_profile.include_lookup_time += src->preprocess_profile.include_lookup_time;
	dst->preprocess_profile.include_child_preprocess_time += src->preprocess_profile.include_child_preprocess_time;
	dst->preprocess_profile.include_emit_time += src->preprocess_profile.include_emit_time;
	dst->preprocess_profile.pragma_time += src->preprocess_profile.pragma_time;
	dst->preprocess_profile.line_time += src->preprocess_profile.line_time;
	dst->preprocess_profile.warning_time += src->preprocess_profile.warning_time;
	dst->preprocess_profile.error_time += src->preprocess_profile.error_time;
	dst->preprocess_profile.skipped_directive_time += src->preprocess_profile.skipped_directive_time;
	dst->preprocess_profile.child_skipped_directive_time += src->preprocess_profile.child_skipped_directive_time;
	dst->preprocess_profile.other_directive_time += src->preprocess_profile.other_directive_time;
	dst->preprocess_profile.files_processed += src->preprocess_profile.files_processed;
	dst->preprocess_profile.lines_processed += src->preprocess_profile.lines_processed;
	dst->preprocess_profile.skipped_lines += src->preprocess_profile.skipped_lines;
	dst->preprocess_profile.directives_seen += src->preprocess_profile.directives_seen;
	dst->preprocess_profile.conditionals_seen += src->preprocess_profile.conditionals_seen;
	dst->preprocess_profile.defines_seen += src->preprocess_profile.defines_seen;
	dst->preprocess_profile.undefs_seen += src->preprocess_profile.undefs_seen;
	dst->preprocess_profile.includes_seen += src->preprocess_profile.includes_seen;
	dst->preprocess_profile.include_empty_sources += src->preprocess_profile.include_empty_sources;
	dst->preprocess_profile.pragmas_seen += src->preprocess_profile.pragmas_seen;
	dst->preprocess_profile.lines_seen += src->preprocess_profile.lines_seen;
	dst->preprocess_profile.warnings_seen += src->preprocess_profile.warnings_seen;
	dst->preprocess_profile.errors_seen += src->preprocess_profile.errors_seen;
	dst->preprocess_profile.skipped_directives_seen += src->preprocess_profile.skipped_directives_seen;
	dst->preprocess_profile.other_directives_seen += src->preprocess_profile.other_directives_seen;
	dst->preprocess_profile.macro_expand_calls += src->preprocess_profile.macro_expand_calls;
	dst->preprocess_profile.tokenize_calls += src->preprocess_profile.tokenize_calls;
	dst->preprocess_profile.object_macro_expansions += src->preprocess_profile.object_macro_expansions;
	dst->preprocess_profile.function_macro_expansions += src->preprocess_profile.function_macro_expansions;
	dst->preprocess_profile.include_cache_hits += src->preprocess_profile.include_cache_hits;
	dst->preprocess_profile.include_cache_misses += src->preprocess_profile.include_cache_misses;
	dst->preprocess_profile.include_file_cache_hits += src->preprocess_profile.include_file_cache_hits;
	dst->preprocess_profile.include_file_cache_misses += src->preprocess_profile.include_file_cache_misses;
	dst->preprocess_profile.include_files_opened += src->preprocess_profile.include_files_opened;
	dst->preprocess_profile.bytes_input += src->preprocess_profile.bytes_input;
	dst->preprocess_profile.bytes_output += src->preprocess_profile.bytes_output;
	for (i = 0; i < PARSER_PROFILE_BUCKET_COUNT; i++) {
		dst->parser_profile.bucket_time[i] += src->parser_profile.bucket_time[i];
		dst->parser_profile.bucket_count[i] += src->parser_profile.bucket_count[i];
	}
}

static double
profile_nanos_to_ms(unsigned long long ns)
{
	return (double)ns / 1000000.0;
}

static void
time_report_print(const char *label, const TimeReport *report)
{
	if (!g_time_report || !report)
		return;

	fprintf(stderr, "tcc time report: %s\n", label);
	fprintf(stderr, "  read:       %8.3f ms\n", report->read_input * 1000.0);
	fprintf(stderr, "  preprocess: %8.3f ms\n", report->preprocess * 1000.0);
	fprintf(stderr, "  parse:      %8.3f ms\n", report->parse * 1000.0);
	fprintf(stderr, "  ast-opt:    %8.3f ms\n", report->ast_opt * 1000.0);
	fprintf(stderr, "  ir-build:   %8.3f ms\n", report->ir_build * 1000.0);
	fprintf(stderr, "  ir-opt:     %8.3f ms\n", report->ir_optimize * 1000.0);
	fprintf(stderr, "  emit:       %8.3f ms\n", report->emit * 1000.0);
	if (report->emit_hybrid_strings > 0.0 || report->emit_hybrid_ir > 0.0 ||
	    report->emit_hybrid_ast > 0.0) {
		fprintf(stderr, "  emit-str:   %8.3f ms\n", report->emit_hybrid_strings * 1000.0);
		fprintf(stderr, "  emit-ir:    %8.3f ms\n", report->emit_hybrid_ir * 1000.0);
		fprintf(stderr, "  emit-ast:   %8.3f ms\n", report->emit_hybrid_ast * 1000.0);
	}
	if (report->emit_ir_collect_strings > 0.0 || report->emit_ir_setup > 0.0 ||
	    report->emit_ir_body > 0.0 || report->emit_ir_debug_finish > 0.0 ||
	    report->emit_ir_named_lookup > 0.0 || report->emit_ir_named_stream > 0.0) {
		fprintf(stderr, "  ir-str:     %8.3f ms\n", report->emit_ir_collect_strings * 1000.0);
		fprintf(stderr, "  ir-setup:   %8.3f ms\n", report->emit_ir_setup * 1000.0);
		fprintf(stderr, "  ir-body:    %8.3f ms\n", report->emit_ir_body * 1000.0);
		fprintf(stderr, "  ir-debug:   %8.3f ms\n", report->emit_ir_debug_finish * 1000.0);
		fprintf(stderr, "  ir-nlook:   %8.3f ms\n", report->emit_ir_named_lookup * 1000.0);
		fprintf(stderr, "  ir-nemit:   %8.3f ms\n", report->emit_ir_named_stream * 1000.0);
	}
	fprintf(stderr, "  assemble:   %8.3f ms\n", report->assemble * 1000.0);
	fprintf(stderr, "  link:       %8.3f ms\n", report->link * 1000.0);
	fprintf(stderr, "  dsymutil:   %8.3f ms\n", report->dsymutil * 1000.0);
	fprintf(stderr, "  total:      %8.3f ms\n", report->total * 1000.0);
	if (report->preprocess_profile.files_processed > 0) {
		fprintf(stderr, "  pp-trigraph %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.trigraph_time));
		fprintf(stderr, "  pp-ctrigra: %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_trigraph_time));
		fprintf(stderr, "  pp-join:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.join_time));
		fprintf(stderr, "  pp-cjoin:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_join_time));
		fprintf(stderr, "  pp-strip:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.strip_comments_time));
		fprintf(stderr, "  pp-cstrip:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_strip_comments_time));
		fprintf(stderr, "  pp-expand:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.macro_expand_time));
		fprintf(stderr, "  pp-cexpnd:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_macro_expand_time));
		fprintf(stderr, "  pp-etok:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.macro_tokenize_time));
		fprintf(stderr, "  pp-cetok:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_macro_tokenize_time));
		fprintf(stderr, "  pp-eobj:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.object_macro_time));
		fprintf(stderr, "  pp-ceobj:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_object_macro_time));
		fprintf(stderr, "  pp-efn:     %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.function_macro_time));
		fprintf(stderr, "  pp-cefn:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_function_macro_time));
		fprintf(stderr, "  pp-efnarg:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.function_macro_arg_collect_time));
		fprintf(stderr, "  pp-cefarg:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_function_macro_arg_collect_time));
		fprintf(stderr, "  pp-efnexp:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.function_macro_arg_expand_time));
		fprintf(stderr, "  pp-cefexp:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_function_macro_arg_expand_time));
		fprintf(stderr, "  pp-efnbld:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.function_macro_build_time));
		fprintf(stderr, "  pp-cefbld:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_function_macro_build_time));
		fprintf(stderr, "  pp-efntail: %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.function_macro_tail_time));
		fprintf(stderr, "  pp-ceftail: %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_function_macro_tail_time));
		fprintf(stderr, "  pp-erscn:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.macro_rescan_time));
		fprintf(stderr, "  pp-cerscn:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_macro_rescan_time));
		fprintf(stderr, "  pp-direct:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.directive_time));
		fprintf(stderr, "  pp-cdirec:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_directive_time));
		fprintf(stderr, "  pp-dnorm:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.directive_normalize_time));
		fprintf(stderr, "  pp-cond:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.conditional_time));
		fprintf(stderr, "  pp-ccond:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_conditional_time));
		fprintf(stderr, "  pp-define:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.define_time));
		fprintf(stderr, "  pp-cdefin:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_define_time));
		fprintf(stderr, "  pp-undef:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.undef_time));
		fprintf(stderr, "  pp-include: %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.include_time));
		fprintf(stderr, "  pp-cincl:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_include_time));
		fprintf(stderr, "  pp-ilookup: %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.include_lookup_time));
		fprintf(stderr, "  pp-ichild:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.include_child_preprocess_time));
		fprintf(stderr, "  pp-iemit:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.include_emit_time));
		fprintf(stderr, "  pp-pragma:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.pragma_time));
		fprintf(stderr, "  pp-line:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.line_time));
		fprintf(stderr, "  pp-warn:    %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.warning_time));
		fprintf(stderr, "  pp-error:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.error_time));
		fprintf(stderr, "  pp-skipdir: %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.skipped_directive_time));
		fprintf(stderr, "  pp-cskipd:  %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.child_skipped_directive_time));
		fprintf(stderr, "  pp-other:   %8.3f ms\n", profile_nanos_to_ms(report->preprocess_profile.other_directive_time));
		fprintf(stderr, "  pp-files:   %8lu\n", report->preprocess_profile.files_processed);
		fprintf(stderr, "  pp-lines:   %8lu\n", report->preprocess_profile.lines_processed);
		fprintf(stderr, "  pp-skip:    %8lu\n", report->preprocess_profile.skipped_lines);
		fprintf(stderr, "  pp-direct#: %8lu\n", report->preprocess_profile.directives_seen);
		fprintf(stderr, "  pp-cond#:   %8lu\n", report->preprocess_profile.conditionals_seen);
		fprintf(stderr, "  pp-define#: %8lu\n", report->preprocess_profile.defines_seen);
		fprintf(stderr, "  pp-undef#:  %8lu\n", report->preprocess_profile.undefs_seen);
		fprintf(stderr, "  pp-includes:%8lu\n", report->preprocess_profile.includes_seen);
		fprintf(stderr, "  pp-iempty:  %8lu\n", report->preprocess_profile.include_empty_sources);
		fprintf(stderr, "  pp-pragma#: %8lu\n", report->preprocess_profile.pragmas_seen);
		fprintf(stderr, "  pp-line#:   %8lu\n", report->preprocess_profile.lines_seen);
		fprintf(stderr, "  pp-warn#:   %8lu\n", report->preprocess_profile.warnings_seen);
		fprintf(stderr, "  pp-error#:  %8lu\n", report->preprocess_profile.errors_seen);
		fprintf(stderr, "  pp-skipd#:  %8lu\n", report->preprocess_profile.skipped_directives_seen);
		fprintf(stderr, "  pp-other#:  %8lu\n", report->preprocess_profile.other_directives_seen);
		fprintf(stderr, "  pp-expand#: %8lu\n", report->preprocess_profile.macro_expand_calls);
		fprintf(stderr, "  pp-tokenize:%8lu\n", report->preprocess_profile.tokenize_calls);
		fprintf(stderr, "  pp-objmac:  %8lu\n", report->preprocess_profile.object_macro_expansions);
		fprintf(stderr, "  pp-fnmac:   %8lu\n", report->preprocess_profile.function_macro_expansions);
		fprintf(stderr, "  pp-icache:  %8lu\n", report->preprocess_profile.include_cache_hits);
		fprintf(stderr, "  pp-imiss:   %8lu\n", report->preprocess_profile.include_cache_misses);
		fprintf(stderr, "  pp-fcache:  %8lu\n", report->preprocess_profile.include_file_cache_hits);
		fprintf(stderr, "  pp-fmiss:   %8lu\n", report->preprocess_profile.include_file_cache_misses);
		fprintf(stderr, "  pp-opened:  %8lu\n", report->preprocess_profile.include_files_opened);
		fprintf(stderr, "  pp-in-bytes:%8lu\n", report->preprocess_profile.bytes_input);
		fprintf(stderr, "  pp-out-bytes:%7lu\n", report->preprocess_profile.bytes_output);
	}
	for (int i = 0; i < PARSER_PROFILE_BUCKET_COUNT; i++) {
		if (report->parser_profile.bucket_time[i] <= 0.0)
			continue;
		fprintf(stderr, "  %-15s %8.3f ms (%lu)\n",
		        parser_profile_bucket_name((ParserProfileBucket)i),
		        report->parser_profile.bucket_time[i] * 1000.0,
		        report->parser_profile.bucket_count[i]);
	}
}


static int
is_valid_m68k_cpu(const char *cpu)
{
	return STRCMP(cpu, "68000") == 0 ||
	       STRCMP(cpu, "68010") == 0 ||
	       STRCMP(cpu, "68020") == 0 ||
	       STRCMP(cpu, "68030") == 0 ||
	       STRCMP(cpu, "68040") == 0;
}

static const char *
asm_dialect_name(AsmDialect dialect)
{
	switch (dialect) {
	case ASM_DIALECT_NASM:
		return "nasm";
	case ASM_DIALECT_GAS_ATT:
		return "att";
	case ASM_DIALECT_GAS_INTEL:
		return "gas";
	case ASM_DIALECT_DEFAULT:
	default:
		return "default";
	}
}

static void
trace_argv(const char *phase, char *const argv[])
{
	int i;

	if (!g_trace_phases)
		return;

	fprintf(stderr, "tcc %s phase:", phase);
	for (i = 0; argv[i]; i++)
		fprintf(stderr, " %s", argv[i]);
	fprintf(stderr, "\n");
}

static void
trace_cc_phase(const char *input, const char *output, const Options *opts,
               Codegen *cg, AsmDialect dialect)
{
	const char *target_name = "unknown";
	const char *std_name = "unknown";

	if (!g_trace_phases)
		return;

	if (cg == &arm64_codegen)
		target_name = "arm64";
	else if (cg == &mips_codegen)
		target_name = "mips";
	else if (cg == &m68k_codegen)
		target_name = "m68k";
	else if (cg == &x64_codegen)
		target_name = "x64";
	else if (cg == &x86_codegen)
		target_name = "x86";

	switch (tcc_lang_standard) {
	case LANG_C89: std_name = "c89"; break;
	case LANG_C90: std_name = "c90"; break;
	case LANG_C99: std_name = "c99"; break;
	case LANG_C11: std_name = "c11"; break;
	case LANG_C17: std_name = "c17"; break;
	case LANG_C23: std_name = "c23"; break;
	default: break;
	}

	fprintf(stderr, "tcc cc phase:\n");
	fprintf(stderr, "  input: %s\n", input);
	fprintf(stderr, "  target: %s\n", target_name);
	fprintf(stderr, "  asm: %s\n", asm_dialect_name(dialect));
	fprintf(stderr, "  opt: -O%d\n", opts->optimize_level);
	fprintf(stderr, "  std: %s\n", std_name);
	fprintf(stderr, "  debug: %s\n", opts->debug ? "yes" : "no");
	if (output)
		fprintf(stderr, "  output: %s\n", output);
	else
		fprintf(stderr, "  output: stdout\n");
}

static char *
default_obj_output_name(const char *input)
{
	size_t input_len = strlen(input);
	size_t base_index = 0;
	size_t stem_len;
	size_t i;

	for (i = 0; i < input_len; i++) {
		if (input[i] == '/')
			base_index = i + 1;
	}

	stem_len = input_len - base_index;
	for (i = base_index; i < input_len; i++) {
		if (input[i] == '.')
			stem_len = i - base_index;
	}

	char *out = xmalloc(stem_len + 3);
	for (i = 0; i < stem_len; i++)
		out[i] = input[base_index + i];
	memcpy(out + stem_len, ".o", 3);
	return out;
}

static char *
default_asm_output_name(const char *input)
{
	size_t input_len = strlen(input);
	size_t base_index = 0;
	size_t stem_len;
	size_t i;

	for (i = 0; i < input_len; i++) {
		if (input[i] == '/')
			base_index = i + 1;
	}

	stem_len = input_len - base_index;
	for (i = base_index; i < input_len; i++) {
		if (input[i] == '.')
			stem_len = i - base_index;
	}

	char *out = xmalloc(stem_len + 3);
	for (i = 0; i < stem_len; i++)
		out[i] = input[base_index + i];
	memcpy(out + stem_len, ".s", 3);
	return out;
}

static int
run_subprocess(char *const argv[])
{
	int pid = fork();
	int status = 0;
	if (pid < 0) {
		tcc_error("fork failed\n");
		return -1;
	}
	if (pid == 0) {
		execvp(argv[0], argv);
		perror(argv[0]);
		_exit(127);
	}
	for (;;) {
		if (waitpid(pid, &status, 0) >= 0)
			break;
		if (errno == EINTR)
			continue;
		tcc_error("waitpid failed for %s (pid=%d, errno=%d: %s)\n",
		          argv[0], pid, errno, strerror(errno));
		return -1;
	}
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int
run_dsymutil(const char *path)
{
#ifdef __APPLE__
	char *argv[3];
	argv[0] = "dsymutil";
	argv[1] = (char *)path;
	argv[2] = NULL;
	trace_argv("dsymutil", argv);
	return run_subprocess(argv);
#else
	(void)path;
	return 0;
#endif
}

static char *
make_temp_asm(void)
{
	char base[] = "/tmp/tccXXXXXX";
	int fd = mkstemp(base);
	if (fd < 0) {
		tcc_error("failed to create temporary file\n");
		return NULL;
	}
	close(fd);
	size_t len = strlen(base);
	char *tmp = xmalloc(len + 3);
	memcpy(tmp, base, len);
	memcpy(tmp + len, ".s", 3);
	if (rename(base, tmp) != 0) {
		xfree(tmp);
		tmp = xstrdup(base);
	}
	return tmp;
}

static char *
make_temp_obj(void)
{
	char base[] = "/tmp/tccXXXXXX";
	int fd = mkstemp(base);
	if (fd < 0) {
		tcc_error("failed to create temporary file\n");
		return NULL;
	}
	close(fd);
	size_t len = strlen(base);
	char *tmp = xmalloc(len + 3);
	memcpy(tmp, base, len);
	memcpy(tmp + len, ".o", 3);
	if (rename(base, tmp) != 0) {
		xfree(tmp);
		tmp = xstrdup(base);
	}
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

	int size = (int)ftell(file);
	if (size < 0) {
		perror("ftell");
		fclose(file);
		exit(1);
	}

	rewind(file);

	char *buffer = xcalloc(1, (size_t)size + 1);
	unsigned int nread = (unsigned int)fread(buffer, 1, (size_t)size, file);
	if (nread != (unsigned int)size) {
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
	        "Usage: %s [options] file.c [file2.c ...]\n"
	        "Options:\n"
	        "  -target=x86|x64|x86_64|arm64|mips|m68k   Select output target (default: host)\n  -mcpu=68000|68010|68020|68030|68040  Select m68k CPU flavour\n"
	        "  -asm=nasm|gas|att               Select assembly dialect (x86)\n"
	        "  -###                Print compiler/as/ld phases as they execute\n"
	        "  -ftime-report       Print per-file and total phase timings\n"
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
	        "  -S                  Emit assembly to <input>.s unless -o is used; -o - writes stdout\n"
	        "  -E                  Preprocess only\n"
	        "  -boot               Use project-local bootstrap headers (cc/include)\n"
	        "  -nostdinc           Do not add implicit standard include directories\n"
	        "  -isystem <dir>      Add a system include directory\n"
	        "  --version, -v, -V   Print compiler version\n"
	        "  -g                  Emit DWARF debug info where supported\n"
	        "  -gdwarf-5          Emit DWARF v5 debug info (default for -g)\n"
	        "  -O, -O0, -O1, -O2   Accept/record optimization level\n"
	        "  -std=c89|c90|c99|c11|c17|c18|c23|c2x|iso9899:<year>  Select C language standard\n",
	        argv0);
}

/* fd of the current stdout redirect (-1 = not redirected). Using int avoids
 * the FILE* / void* 4-vs-8 byte store bug in stage1 arm64 codegen. */
static int g_redirect_fd = -1;

static int
redirect_stdout_to(const char *path)
{
	fflush(stdout);
	int saved = dup(1);
	if (saved < 0) { perror("dup"); return -1; }

	FILE *fp = fopen(path, "w");
	if (!fp) {
		perror(path);
		close(saved);
		return -1;
	}
	int fd = fileno(fp);
	if (dup2(fd, 1) < 0) {
		perror("dup2");
		fclose(fp);
		close(saved);
		return -1;
	}
	/* Keep fd open for writing (stdout now points to it via fd 1);
	 * close the FILE wrapper but leave the underlying fd alive. */
	fclose(fp); /* decrements refcount; fd 1 still open */
	g_redirect_fd = fd; /* remember for restore (though fd == 1 after dup2) */
	return saved;
}

static void
restore_stdout(int saved_fd)
{
	fflush(stdout);
	/* Close the asm file (fd 1 currently points to it) */
	if (g_redirect_fd >= 0) { g_redirect_fd = -1; }
	if (saved_fd >= 0) { dup2(saved_fd, 1); close(saved_fd); }
}

static char *
codegen_preprocess_input(const char *input, char *source,
                         TimeReport *time_report, int collect_profile)
{
	double t0;
	char *expanded;

	trace_main_boot("codegen_preprocess_input enter");
	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s preprocess\n", input);
	t0 = tcc_monotonic_seconds();
	preprocess_profile_enable(collect_profile);
	expanded = preprocess(input, source);
	trace_main_boot("codegen_preprocess_input exit");
	if (time_report)
		time_report->preprocess += tcc_monotonic_seconds() - t0;
	if (collect_profile)
		preprocess_profile_get(&time_report->preprocess_profile);
	return expanded;
}

static Node *
codegen_parse_program(const char *input, char *expanded, const Options *opts,
                      TimeReport *time_report, int collect_profile)
{
	double t0;
	Node *program;

	parser_profile_enabled_flag = collect_profile ? 1 : 0;
	parser_emit_debug = opts->debug ? 1 : 0;
	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s parse\n", input);
	t0 = tcc_monotonic_seconds();
	program = parse_program(input, expanded);
	if (time_report)
		time_report->parse += tcc_monotonic_seconds() - t0;
	if (collect_profile)
		parser_profile_get(&time_report->parser_profile);
	return program;
}

static IRProgram *
codegen_build_ir_program(const char *input, Node **program_io, Codegen *cg,
                         int optimize_level, TimeReport *time_report)
{
	double t0;
	IRProgram *ir;

	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s ast-opt\n", input);
	t0 = tcc_monotonic_seconds();
	*program_io = fold_constants(*program_io);
	*program_io = eliminate_dead_code(*program_io);
	if (time_report)
		time_report->ast_opt += tcc_monotonic_seconds() - t0;

	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s ir-build\n", input);
	t0 = tcc_monotonic_seconds();
	ir_set_target_codegen(cg);
	ir = ir_build(*program_io);
	if (time_report)
		time_report->ir_build += tcc_monotonic_seconds() - t0;

	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s ir-opt\n", input);
	t0 = tcc_monotonic_seconds();
	ir_optimize(ir, optimize_level);
	if (time_report)
		time_report->ir_optimize += tcc_monotonic_seconds() - t0;

	return ir;
}

static int
codegen_emit_phase(const char *input, Codegen *cg, const Options *opts,
                   Node **program_io, IRProgram *ir,
                   int dump, int dump_ir, int dump_cfg,
                   int require_ir, int trace_codegen,
                   int strict_ir_except_structs,
                   TimeReport *time_report)
{
	double t0;
	int ok = 1;
	Node *program = *program_io;
	int allow_arm64_direct_aggregate_hybrid = 0;
	HybridEmitProfile hybrid_profile = {0};
	IREmitProfile ir_emit_profile = {0};

	if (cg == &arm64_codegen &&
	    ir_has_unsupported(ir)) {
		const char *reason = ir_unsupported_reason(ir);

		if (reason &&
		    strstr(reason, "arm64 direct aggregate function ABI") != NULL)
			allow_arm64_direct_aggregate_hybrid = 1;
	}

	if (ir_can_codegen(ir)) {
		free_ast(program);
		program = NULL;
		*program_io = NULL;
	}

	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s emit\n", input);
	t0 = tcc_monotonic_seconds();
	if (dump) {
		dump_ast(program, 0);
	} else if (dump_ir) {
		ir_dump(ir);
	} else if (dump_cfg) {
		ir_dump_cfg(ir);
	} else if (ir_can_codegen(ir)) {
		ir_emit_profile_reset();
		if (opts->debug && trace_codegen)
			fprintf(stderr, "[tcc] codegen=IR\n");
		ir_emit_program(ir, cg, opts->debug);
		ir_emit_profile_get(&ir_emit_profile);
	} else if ((!require_ir || allow_arm64_direct_aggregate_hybrid) &&
	           !strict_ir_except_structs &&
	           !opts->debug &&
	           cg == &arm64_codegen &&
	           ir_has_unsupported(ir) &&
	           (!ir_has_unsupported_outside_functions(ir) ||
	            allow_arm64_direct_aggregate_hybrid)) {
		if (trace_codegen) {
			fprintf(stderr, "[tcc] codegen=IR+AST hybrid");
			if (ir_has_unsupported(ir))
				fprintf(stderr, ": %s", ir_unsupported_reason(ir));
			trace_unsupported_functions(ir);
			fprintf(stderr, "\n");
		}
		emit_program_hybrid(program, ir, cg, &hybrid_profile);
	} else if (require_ir || (strict_ir_except_structs &&
	                          (!ir_has_unsupported(ir) ||
	                           strstr(ir_unsupported_reason(ir), "struct") == NULL))) {
		fprintf(stderr, "IR lowering/codegen incomplete");
		if (ir_has_unsupported(ir))
			fprintf(stderr, ": %s", ir_unsupported_reason(ir));
		fprintf(stderr, "\n");
		ok = 0;
	} else if (ir_has_unsupported(ir) &&
	           strstr(ir_unsupported_reason(ir), "floating-point") != NULL &&
	           cg != &arm64_codegen) {
		fprintf(stderr, "IR lowering/codegen incomplete: %s\n",
		        ir_unsupported_reason(ir));
		ok = 0;
	} else {
		if (opts->debug && trace_codegen) {
			fprintf(stderr, "[tcc] codegen=AST fallback");
			if (ir_has_unsupported(ir))
				fprintf(stderr, ": %s", ir_unsupported_reason(ir));
			fprintf(stderr, "\n");
		}
		emit_program(program, cg);
	}
	if (time_report)
		time_report->emit += tcc_monotonic_seconds() - t0;
	if (time_report) {
		time_report->emit_hybrid_strings += hybrid_profile.string_literals;
		time_report->emit_hybrid_ir += hybrid_profile.ir_functions;
		time_report->emit_hybrid_ast += hybrid_profile.ast_fallback_functions;
		time_report->emit_ir_collect_strings += ir_emit_profile.collect_strings;
		time_report->emit_ir_setup += ir_emit_profile.setup;
		time_report->emit_ir_body += ir_emit_profile.body;
		time_report->emit_ir_debug_finish += ir_emit_profile.debug_finish;
		time_report->emit_ir_named_lookup += ir_emit_profile.named_lookup;
		time_report->emit_ir_named_stream += ir_emit_profile.named_stream;
	}

	return ok;
}

/*
 * codegen_one - run the full parse → IR → codegen pipeline for one source file.
 * Assembly is written to stdout (caller has already redirected it if needed).
 * Returns 1 on success, 0 on error.
 */
static int
codegen_one(const char *input, Codegen *cg, const Options *opts,
            int dump, int dump_ir, int dump_cfg,
            int require_ir, int trace_codegen, int strict_ir_except_structs,
            TimeReport *time_report)
{
	int collect_profile = (time_report && g_time_report);

	trace_main_boot("codegen_one enter");
	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s read\n", input);
	double total_start = tcc_monotonic_seconds();
	double t0 = tcc_monotonic_seconds();
	char *source = read_file(input);
	trace_main_boot("codegen_one read_file done");
	if (time_report)
		time_report->read_input += tcc_monotonic_seconds() - t0;

	char *expanded = codegen_preprocess_input(input, source, time_report,
	                                          collect_profile);
	trace_main_boot("codegen_one after preprocess");

	if (opts->preprocess_only) {
		const char *skip_output = getenv("TCC_SKIP_PP_ONLY_OUTPUT");
		if (g_trace_phases)
			fprintf(stderr, "tcc cc step: %s preprocess-only output\n", input);
		if (!skip_output || !skip_output[0])
		fputs(expanded, stdout);
		if (g_trace_phases)
			fprintf(stderr, "tcc cc step: %s preprocess-only free expanded\n", input);
		xfree(expanded);
		if (g_trace_phases)
			fprintf(stderr, "tcc cc step: %s preprocess-only free source\n", input);
		xfree(source);
		if (time_report)
			time_report->total += tcc_monotonic_seconds() - total_start;
		return 1;
	}

	Node *program = codegen_parse_program(input, expanded, opts, time_report,
	                                      collect_profile);
	IRProgram *ir = codegen_build_ir_program(input, &program, cg,
	                                         opts->optimize_level, time_report);
	int ok = codegen_emit_phase(input, cg, opts, &program, ir,
	                            dump, dump_ir, dump_cfg,
	                            require_ir, trace_codegen,
	                            strict_ir_except_structs,
	                            time_report);

	if (g_trace_phases)
		fprintf(stderr, "tcc cc step: %s cleanup\n", input);
	ir_free(ir);
	if (program)
		free_ast(program);
	xfree(expanded);
	xfree(source);
	if (time_report)
		time_report->total += tcc_monotonic_seconds() - total_start;
	return ok;
}

/*
 * assemble_obj - run the system assembler on asm_path to produce obj_path.
 * Returns 0 on success, non-zero on failure.
 */
static int
assemble_obj(const char *asm_path, const char *obj_path)
{
	char *as_argv[6];
	as_argv[0] = "cc";
	as_argv[1] = "-c";
	as_argv[2] = (char *)asm_path;
	as_argv[3] = "-o";
	as_argv[4] = (char *)obj_path;
	as_argv[5] = NULL;
	trace_argv("as", as_argv);
	return run_subprocess(as_argv);
}

static void
append_string_arg(char ***args, int *count, int *cap, const char *arg)
{
	if (*count >= *cap) {
		int new_cap = *cap ? *cap * 2 : 8;
		*args = (char **)xrealloc(*args, (size_t)new_cap * sizeof(char *));
		*cap = new_cap;
	}
	(*args)[(*count)++] = xstrdup(arg);
}

static void
free_string_args(char **args, int count)
{
	while (count > 0) {
		xfree(args[count - 1]);
		count--;
	}
	xfree(args);
}

typedef struct {
	Codegen *cg;
	const Options *opts;
	AsmDialect requested_dialect;
	int dump;
	int dump_ir;
	int dump_cfg;
	int require_ir;
	int trace_codegen;
	int strict_ir_except_structs;
	char **inputs;
	int input_count;
	char **link_args;
	int link_arg_count;
	TimeReport *total_time;
	double overall_start;
} DriverContext;

static int
emit_input_to_asm_file(const DriverContext *ctx,
                       const char *input,
                       const char *asm_path,
                       int reset_file_macros,
                       TimeReport *file_time)
{
	int saved;
	int ok;

	if (reset_file_macros)
		preprocess_reset_file_macros();
	trace_main_boot("emit_input_to_asm_file before redirect");

	saved = redirect_stdout_to(asm_path);
	if (saved < 0) {
		return 0;
	}
	trace_main_boot("emit_input_to_asm_file after redirect");

	trace_cc_phase(input, asm_path, ctx->opts, ctx->cg, ctx->requested_dialect);
	trace_main_boot("emit_input_to_asm_file before codegen_one");

	ok = codegen_one(input,
	                 ctx->cg,
	                 ctx->opts,
	                 ctx->dump,
	                 ctx->dump_ir,
	                 ctx->dump_cfg,
	                 ctx->require_ir,
	                 ctx->trace_codegen,
	                 ctx->strict_ir_except_structs,
	                 file_time);
	trace_main_boot("emit_input_to_asm_file after codegen_one");
	restore_stdout(saved);
	trace_main_boot("emit_input_to_asm_file after restore");

	if (!ok)
		return 0;

	if (ctx->cg == &arm64_codegen &&
	    (!ctx->opts->bootstrap_includes ||
	     arm64_bootstrap_peephole_enabled()))
		arm64_peephole_optimize_file(asm_path);

	return 1;
}

static char *
compile_input_to_temp_object(const DriverContext *ctx,
                             const char *input,
                             int reset_file_macros,
                             TimeReport *file_time)
{
	char *temp_asm;
	char *temp_obj;
	double t0;
	int ret;

	temp_asm = make_temp_asm();
	temp_obj = make_temp_obj();
	if (!emit_input_to_asm_file(ctx, input, temp_asm, reset_file_macros, file_time)) {
		remove(temp_asm);
		xfree(temp_asm);
		xfree(temp_obj);
		return NULL;
	}

	t0 = tcc_monotonic_seconds();
	ret = assemble_obj(temp_asm, temp_obj);
	file_time->assemble += tcc_monotonic_seconds() - t0;
	remove(temp_asm);
	xfree(temp_asm);

	if (ret != 0)
		tcc_error("Assembler failed on %s\n", input);

	file_time->total += file_time->assemble;
	time_report_add(ctx->total_time, file_time);
	time_report_print(input, file_time);
	return temp_obj;
}

static int
compile_input_to_named_object(const DriverContext *ctx,
                              const char *input,
                              const char *obj_name,
                              int reset_file_macros,
                              TimeReport *file_time)
{
	char *temp_asm;
	double t0;
	int ret;

	temp_asm = make_temp_asm();
	if (!emit_input_to_asm_file(ctx, input, temp_asm, reset_file_macros, file_time)) {
		remove(temp_asm);
		xfree(temp_asm);
		return 0;
	}

	t0 = tcc_monotonic_seconds();
	ret = assemble_obj(temp_asm, obj_name);
	file_time->assemble += tcc_monotonic_seconds() - t0;
	remove(temp_asm);
	xfree(temp_asm);

	if (ret != 0)
		tcc_error("Assembler failed on %s\n", input);

	file_time->total += file_time->assemble;
	time_report_add(ctx->total_time, file_time);
	time_report_print(input, file_time);
	return 1;
}

static int
link_temp_objects(const DriverContext *ctx,
                  char **temp_objs,
                  int n_compiled,
                  const char *link_output)
{
	int ld_argc = n_compiled + ctx->link_arg_count + 4;
	char **ld_argv = (char **)xcalloc((size_t)ld_argc, sizeof(char *));
	int pos = 0;
	int k;
	double t0;
	int ret;

	ld_argv[pos++] = "cc";
	for (k = 0; k < n_compiled; k++)
		ld_argv[pos++] = temp_objs[k];
	for (k = 0; k < ctx->link_arg_count; k++)
		ld_argv[pos++] = ctx->link_args[k];
	ld_argv[pos++] = "-o";
	ld_argv[pos++] = (char *)link_output;
	ld_argv[pos] = NULL;

	trace_argv("ld", ld_argv);
	t0 = tcc_monotonic_seconds();
	ret = run_subprocess(ld_argv);
	ctx->total_time->link += tcc_monotonic_seconds() - t0;
	xfree(ld_argv);
	if (ret != 0) {
		tcc_error("Link failed\n");
		return 0;
	}
	return 1;
}

static int __attribute__((noinline))
finalize_link_job(const DriverContext *ctx,
                  const char *link_output,
                  char **temp_objs,
                  int n_compiled,
                  int link_ok)
{
	int preserve_debug_objs = 0;
	int k;

	if (link_ok && ctx->opts->debug) {
		{
			double t0 = tcc_monotonic_seconds();
			int ret = run_dsymutil(link_output);
			ctx->total_time->dsymutil += tcc_monotonic_seconds() - t0;
			if (ret != 0) {
				fprintf(stderr,
				        "tcc: warning: dsymutil failed; preserving temporary object files for debug info\n");
				preserve_debug_objs = 1;
			}
		}
	}

	for (k = 0; k < n_compiled; k++) {
		if (!preserve_debug_objs)
			remove(temp_objs[k]);
		xfree(temp_objs[k]);
	}

	xfree(temp_objs);
	ctx->total_time->total = tcc_monotonic_seconds() - ctx->overall_start;
	time_report_print("total", ctx->total_time);
	free_string_args(ctx->link_args, ctx->link_arg_count);
	xfree(ctx->inputs);
	return link_ok ? 0 : 1;
}

static int
compile_and_link_inputs(const DriverContext *ctx)
{
	const char *link_output = ctx->opts->output_file ? ctx->opts->output_file : "a.out";
	char **input = ctx->inputs;
	char **input_end = ctx->inputs + ctx->input_count;
	char **temp_objs = (char **)xcalloc((size_t)ctx->input_count, sizeof(char *));
	char **temp_obj_slot = temp_objs;
	int n_compiled = 0;
	int link_ok = 1;

	while (input < input_end) {
		TimeReport file_time = {0};
		char *temp_obj = compile_input_to_temp_object(ctx,
		                                             *input,
		                                             n_compiled > 0,
		                                             &file_time);
		if (!temp_obj) {
			link_ok = 0;
			break;
		}
		*temp_obj_slot++ = temp_obj;
		n_compiled++;
		input++;
	}

	if (link_ok)
		link_ok = link_temp_objects(ctx, temp_objs, n_compiled, link_output);

	return finalize_link_job(ctx, link_output, temp_objs, n_compiled, link_ok);
}

static int
compile_only_inputs(const DriverContext *ctx)
{
	int i;

	trace_main_boot("compile_only_inputs enter");
	for (i = 0; i < ctx->input_count; i++) {
		TimeReport file_time = {0};
		char *generated = NULL;
		const char *obj_name;
		int ok;
		trace_main_boot("compile_only_inputs loop");

		if (ctx->opts->output_file) {
			obj_name = ctx->opts->output_file;
		} else {
			generated = default_obj_output_name(ctx->inputs[i]);
			obj_name = generated;
		}

		ok = compile_input_to_named_object(ctx,
		                                  ctx->inputs[i],
		                                  obj_name,
		                                  i > 0,
		                                  &file_time);
		xfree(generated);
		if (!ok) {
			free_string_args(ctx->link_args, ctx->link_arg_count);
			xfree(ctx->inputs);
			return 1;
		}
	}

	ctx->total_time->total = tcc_monotonic_seconds() - ctx->overall_start;
	time_report_print("total", ctx->total_time);
	free_string_args(ctx->link_args, ctx->link_arg_count);
	xfree(ctx->inputs);
	return 0;
}


int
main(int argc, char **argv)
{
	TimeReport total_time = {0};
	double overall_start = tcc_monotonic_seconds();
	Codegen *cg;
	int pp_target_raw;
#if defined(__aarch64__) || defined(__arm64__)
	cg = &arm64_codegen;
	pp_target_raw = PP_TARGET_ARM64;
#elif defined(__x86_64__) || defined(_M_X64)
	cg = &x64_codegen;
	pp_target_raw = PP_TARGET_X64;
#elif defined(__i386__) || defined(_M_IX86)
	cg = &x86_codegen;
	pp_target_raw = PP_TARGET_X86;
#elif defined(__mips__)
	cg = &mips_codegen;
	pp_target_raw = PP_TARGET_MIPS;
#else
	cg = &x86_codegen;
	pp_target_raw = PP_TARGET_X86;
#endif
	PreprocessTarget pp_target = (PreprocessTarget)pp_target_raw;

	/* Dynamic list of input files */
	char **inputs = NULL;
	int input_count = 0;
	int input_cap = 0;

	char **link_args = NULL;
	int link_arg_count = 0;
	int link_arg_cap = 0;

	int dump = 0;
	int dump_ir = 0;
	int dump_cfg = 0;
	int require_ir = 0;
	int trace_codegen = 0;
	int strict_ir_except_structs = 0;
	int use_bootstrap_includes = 0;
	int use_stdinc = 1;
	Options opts = {0};
	AsmDialect requested_dialect = ASM_DIALECT_DEFAULT;
	LinkModel requested_link_model = LINK_DYNAMIC;
	const char *m68k_cpu_name = "68000";
	int m68k_cpu_explicit = 0;

	trace_main_boot("main enter");

	for (int i = 1; i < argc; i++) {
		if (STRCMP(argv[i], "-debug") == 0) {
			fprintf(stderr,"Debugging enabled\n");
			tcc_set_debug(1);
		} else if (STRCMP(argv[i], "-###") == 0) {
			g_trace_phases = 1;
		} else if (STRCMP(argv[i], "-ftime-report") == 0) {
			g_time_report = 1;
		} else if (STRNCMP(argv[i], "-std=", 5) == 0) {
			int parsed_std = 0;
			const char *std_value = argv[i] + 5;
			if (!parse_lang_standard(std_value, &parsed_std))
				tcc_error("unknown -std value '%s'\n", std_value);
			tcc_lang_standard = (LangStandard)parsed_std;
			tcc_iso_diagnostics = 1;
		} else if (STRCMP(argv[i], "-target=arm64") == 0) {
			cg = &arm64_codegen;
			pp_target = PP_TARGET_ARM64;
		} else if (STRCMP(argv[i], "-target=mips") == 0) {
			cg = &mips_codegen;
			pp_target = PP_TARGET_MIPS;
		} else if (STRCMP(argv[i], "-target=m68k") == 0) {
			cg = &m68k_codegen;
			pp_target = PP_TARGET_M68K;
		} else if (STRCMP(argv[i], "-target=x64") == 0 || STRCMP(argv[i], "-target=x86_64") == 0) {
			cg = &x64_codegen;
			pp_target = PP_TARGET_X64;
		} else if (STRCMP(argv[i], "-target=x86") == 0) {
			cg = &x86_codegen;
			pp_target = PP_TARGET_X86;
		} else if (STRNCMP(argv[i], "-mcpu=", 6) == 0) {
			const char *cpu = argv[i] + 6;
			if (!is_valid_m68k_cpu(cpu))
				tcc_error("unknown m68k -mcpu value '%s'; expected 68000, 68010, 68020, 68030, or 68040\n", cpu);
			m68k_cpu_name = cpu;
			m68k_cpu_explicit = 1;
		} else if (STRCMP(argv[i], "-asm=nasm") == 0) {
			requested_dialect = ASM_DIALECT_NASM;
		} else if (STRCMP(argv[i], "-asm=gas") == 0) {
			requested_dialect = ASM_DIALECT_GAS_INTEL;
		} else if (STRCMP(argv[i], "-asm=att") == 0) {
			requested_dialect = ASM_DIALECT_GAS_ATT;
		} else if (STRNCMP(argv[i], "-asm=", 5) == 0) {
			tcc_error("unknown -asm= value '%s'; expected nasm, gas, or att\n", argv[i] + 5);
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
		} else if (STRCMP(argv[i], "-Xemit-dwarf-scaffold") == 0) {
			opts.emit_dwarf_scaffold = 1;
		} else if (STRCMP(argv[i], "-fir-struct-fallback-only") == 0 || STRCMP(argv[i], "-fir-strict") == 0) {
			strict_ir_except_structs = 1;
		} else if (STRCMP(argv[i], "-c") == 0) {
			opts.compile_only = 1;
		} else if (STRCMP(argv[i], "-g") == 0) {
			opts.debug = 1;
		} else if (STRCMP(argv[i], "-gdwarf-5") == 0) {
			opts.debug = 1;
		} else if (STRCMP(argv[i], "-gdwarf=5") == 0) {
			opts.debug = 1;
		} else if (STRCMP(argv[i], "-gdwarf") == 0) {
			tcc_error("missing DWARF version after -gdwarf; expected -gdwarf-5\n");
		} else if (STRNCMP(argv[i], "-gdwarf-", 8) == 0) {
			tcc_error("DWARF version option '%s' is not supported by the current emitter; use -gdwarf-5\n", argv[i]);
		} else if (STRNCMP(argv[i], "-gdwarf=", 8) == 0) {
			tcc_error("DWARF version option '%s' is not supported by the current emitter; use -gdwarf-5\n", argv[i]);
		} else if (STRCMP(argv[i], "-S") == 0) {
			opts.emit_asm_only = 1;
		} else if (STRCMP(argv[i], "-E") == 0) {
			opts.preprocess_only = 1;
		} else if (STRCMP(argv[i], "-boot") == 0 || STRCMP(argv[i], "--bootstrap-includes") == 0) {
			use_bootstrap_includes = 1;
			opts.bootstrap_includes = 1;
		} else if (STRCMP(argv[i], "-nostdinc") == 0) {
			use_stdinc = 0;
		} else if (STRCMP(argv[i], "-isystem") == 0) {
			if (i + 1 >= argc) { tcc_error("-isystem requires a directory argument"); free_string_args(link_args, link_arg_count); xfree(inputs); return 1; }
			preprocess_set_include_dir(argv[++i]);
		} else if (STRNCMP(argv[i], "-isystem", 8) == 0 && argv[i][8] != '\0') {
			preprocess_set_include_dir(argv[i] + 8);
		} else if (STRCMP(argv[i], "-dD") == 0) {
			preprocess_set_line_markers(1);
		} else if (STRCMP(argv[i], "--version") == 0 ||
		           STRCMP(argv[i], "-v") == 0 ||
		           STRCMP(argv[i], "-V") == 0 ||
		           STRCMP(argv[i], "-version") == 0) {
			printf("tcc version %s\n", TCC_VERSION);
			xfree(inputs);
			return 0;
		} else if (STRCMP(argv[i], "-w") == 0) {
			tcc_set_warnings(0);
		} else if (STRCMP(argv[i], "-Werror") == 0) {
			tcc_set_warnings_as_errors(1);
		} else if (STRNCMP(argv[i], "-Werror=", 8) == 0) {
			/* -Werror=specific-warning: promote that warning to error.
			 * We accept the flag for compatibility; unimplemented warnings
			 * are silently ignored so this is a no-op for most cases. */
			(void)argv[i]; /* accepted, no-op for unimplemented warnings */
		} else if (STRCMP(argv[i], "-Wall") == 0 ||
		           STRCMP(argv[i], "-Wextra") == 0 ||
		           STRCMP(argv[i], "-Wpedantic") == 0 ||
		           STRCMP(argv[i], "-pedantic") == 0) {
			/* Accepted for compatibility; warnings not fully implemented.
			 * Still enable hard ISO constraints that would otherwise remain
			 * extension-compatible in the default dialect. */
			if (STRCMP(argv[i], "-Wpedantic") == 0 ||
			    STRCMP(argv[i], "-pedantic") == 0)
				tcc_iso_diagnostics = 1;
		} else if (STRNCMP(argv[i], "-Wno-", 5) == 0) {
			/* -Wno-xxx: silence a specific warning category.
			 * Accepted silently for compatibility with build systems. */
		} else if (STRNCMP(argv[i], "-W", 2) == 0) {
			/* Unknown -Wxxx flag: accept silently for compatibility. */
		} else if (STRNCMP(argv[i], "-I", 2) == 0) {
			const char *dir = argv[i][2] ? argv[i] + 2 : NULL;
			if (!dir) {
				if (i + 1 >= argc) { tcc_error("-I requires a directory argument"); free_string_args(link_args, link_arg_count); xfree(inputs); return 1; }
				dir = argv[++i];
			}
			preprocess_set_include_dir(dir);
		} else if (STRNCMP(argv[i], "-L", 2) == 0) {
			const char *dir = argv[i][2] ? argv[i] + 2 : NULL;
			char *joined;
			if (!dir) {
				if (i + 1 >= argc) { tcc_error("-L requires a directory argument"); free_string_args(link_args, link_arg_count); xfree(inputs); return 1; }
				dir = argv[++i];
			}
			{
				size_t dir_len = strlen(dir);
				joined = xmalloc(dir_len + 3);
				memcpy(joined, "-L", 2);
				memcpy(joined + 2, dir, dir_len + 1);
			}
			append_string_arg(&link_args, &link_arg_count, &link_arg_cap, joined);
			xfree(joined);
		} else if (STRNCMP(argv[i], "-l", 2) == 0) {
			const char *name = argv[i][2] ? argv[i] + 2 : NULL;
			char *joined;
			if (!name) {
				if (i + 1 >= argc) { tcc_error("-l requires a library name"); free_string_args(link_args, link_arg_count); xfree(inputs); return 1; }
				name = argv[++i];
			}
			{
				size_t name_len = strlen(name);
				joined = xmalloc(name_len + 3);
				memcpy(joined, "-l", 2);
				memcpy(joined + 2, name, name_len + 1);
			}
			append_string_arg(&link_args, &link_arg_count, &link_arg_cap, joined);
			xfree(joined);
		} else if (STRNCMP(argv[i], "-O", 2) == 0) {
			if (argv[i][2] == '\0')
				opts.optimize_level = 1;
			else if (argv[i][2] >= '0' && argv[i][2] <= '9' && argv[i][3] == '\0')
				opts.optimize_level = argv[i][2] - '0';
			else {
				tcc_warn("Unsupported optimization option: %s\n", argv[i]);
				xfree(inputs);
				return 1;
			}
		} else if (STRCMP(argv[i], "-o") == 0) {
			if (i + 1 >= argc) {
				tcc_error("-o requires a filename\n");
				xfree(inputs);
				return 1;
			}
			opts.output_file = argv[++i];
		} else if (argv[i][0] == '-') {
			tcc_error("Unknown option: %s\n", argv[i]);
			usage(argv[0]);
			xfree(inputs);
			return 1;
		} else {
			/* input file */
			if (input_count >= input_cap) {
				int new_cap = input_cap ? input_cap * 2 : 4;
				inputs = (char **)xrealloc(inputs, (size_t)new_cap * sizeof(char *));
				input_cap = new_cap;
			}
			inputs[input_count++] = argv[i];
		}
	}

	if (input_count == 0) {
		usage(argv[0]);
		free_string_args(link_args, link_arg_count);
		xfree(inputs);
		return 1;
	}

	/* -c or -S with multiple inputs and explicit -o is invalid */
	if ((opts.compile_only || opts.emit_asm_only) &&
	    opts.output_file && input_count > 1) {
		tcc_error("cannot use -o with multiple input files and -%c\n",
		          opts.compile_only ? 'c' : 'S');
		xfree(inputs);
		return 1;
	}

	if (requested_dialect == ASM_DIALECT_GAS_ATT) {
		tcc_error("-asm=att not implemented yet\n");
		xfree(inputs);
		return 1;
	}
	if (cg != &m68k_codegen && m68k_cpu_explicit) {
		tcc_error("-mcpu=%s is currently only valid with -target=m68k\n", m68k_cpu_name);
	}
	if (cg == &m68k_codegen)
		m68k_set_cpu_name(m68k_cpu_name);

	codegen_set_asm_dialect(requested_dialect);
	preprocess_set_asm_dialect(requested_dialect);
	codegen_set_link_model(requested_link_model);

	/* Include directory setup.  Explicit -I/-isystem paths were already added
	 * during option parsing.  -boot enables project-local bootstrap stubs;
	 * -nostdinc disables implicit standard include directories.
	 *
	 * We need the binary's own directory to locate cc/include stubs.
	 * argv[0] may be a bare name (no slash) when tcc is found via PATH,
	 * so we resolve the real executable path first.
	 */
	preprocess_set_bootstrap_includes(use_bootstrap_includes);
	preprocess_set_stdinc(use_stdinc);
	if (use_stdinc) {
		const char *env_inc = getenv("TCC_INCLUDE_DIR");
		if (env_inc) {
			preprocess_set_include_dir(env_inc);
		} else {
			/* Resolve the directory containing the tcc binary.
			 * Try /proc/self/exe (Linux), then fall back to argv[0]. */
			char inc_path[512];
			const char *exe = argv[0];
			int dirlen = -1;
			int path_i = 0;

#if defined(__linux__)
			{
				static char exe_buf[512];
				int n = (int)readlink("/proc/self/exe", exe_buf,
				                      sizeof(exe_buf) - 1);
				if (n > 0) {
					exe_buf[n] = '\0';
					exe = exe_buf;
				}
			}
#endif
			path_i = 0;
			while (exe[path_i]) {
				if (exe[path_i] == '/')
					dirlen = path_i;
				path_i++;
			}

			if (dirlen >= 0) {
				int k, j;
				if (dirlen > 500) dirlen = 500;

				if (use_bootstrap_includes) {
					/* Bootstrap: cc/include stubs are the primary headers. */
					k = 0;
					while (k < dirlen) { inc_path[k] = exe[k]; k++; }
					j = 0;
					const char *stub_suffix = "/../cc/include";
					while (stub_suffix[j] && k < 511) inc_path[k++] = stub_suffix[j++];
					inc_path[k] = '\0';
					preprocess_set_include_dir(inc_path);
				}

				if (use_bootstrap_includes) {
					/* <bindir>/../include/tcc (installed bootstrap headers) */
					k = 0;
					while (k < dirlen) { inc_path[k] = exe[k]; k++; }
					j = 0;
					const char *inst_suffix = "/../include/tcc";
					while (inst_suffix[j] && k < 511) inc_path[k++] = inst_suffix[j++];
					inc_path[k] = '\0';
					preprocess_set_include_dir(inc_path);
				}
			} else {
				/* argv[0] had no slash and /proc/self/exe unavailable;
				 * fall back to hardcoded install locations. */
				if (use_bootstrap_includes)
					preprocess_set_include_dir("/usr/local/include/tcc");
			}

			if (!use_bootstrap_includes) {
				/*
				 * System header search paths for non-bootstrap builds.
				 * On macOS: use SDKROOT when set, otherwise fall back to the
				 * standard Xcode / CommandLineTools SDK include directories.
				 * On Linux/other: /usr/local/include then /usr/include.
				 * After system paths, re-add cc/include as a low-priority
				 * fallback for compiler-internal headers (stdarg.h, stddef.h,
				 * stdbool.h) that the system SDK does not provide directly.
				 */
#if defined(__APPLE__)
				add_macos_sdk_include_dir();
				preprocess_set_include_dir("/usr/local/include");
				preprocess_set_include_dir("/usr/include");
#else
				preprocess_set_include_dir("/usr/local/include");
				preprocess_set_include_dir("/usr/include");
#endif
				/* Low-priority fallback for compiler-internal headers.  Keep
				 * this after the SDK/system paths so real system headers win,
				 * then fall back to compiler-provided stdarg.h/stddef.h/
				 * stdbool.h when the platform SDK does not ship them as
				 * standalone headers. */
				if (dirlen >= 0) {
					int k, j;
					k = 0;
					while (k < dirlen) { inc_path[k] = exe[k]; k++; }
					j = 0;
					const char *s = "/../cc/include";
					while (s[j] && k < 511) inc_path[k++] = s[j++];
					inc_path[k] = '\0';
					preprocess_set_include_dir(inc_path);
				}
				preprocess_set_include_dir("/usr/local/include/tcc");
			}
		}
	}

	if (opts.debug && cg == &arm64_codegen)
		arm64_set_debug(1);
	if (opts.debug && cg == &x64_codegen)
		x64_set_debug(1);
	if (opts.debug && cg == &x86_codegen)
		x86_set_debug(1);

	preprocess_configure(pp_target);
	if (!opts.preprocess_only)
		preprocess_set_line_markers(1);

	/* ---------------------------------------------------------------
	 * Dispatch based on mode
	 * --------------------------------------------------------------- */

	/* -E: preprocess only — handle each file in sequence */
	if (opts.preprocess_only) {
		int i = 0;
		int saved = -1;
		int redirected = 0;

		if (opts.output_file && STRCMP(opts.output_file, "-") != 0) {
			saved = redirect_stdout_to(opts.output_file);
			if (saved < 0) {
				free_string_args(link_args, link_arg_count);
				xfree(inputs);
				return 1;
			}
			redirected = 1;
		}

		for (i = 0; i < input_count; i++) {
			TimeReport file_time = {0};
			if (i > 0) preprocess_reset_file_macros();
			if (!codegen_one(inputs[i], cg, &opts, dump, dump_ir, dump_cfg,
			                 require_ir, trace_codegen, strict_ir_except_structs,
			                 &file_time)) {
				if (redirected)
					restore_stdout(saved);
				free_string_args(link_args, link_arg_count);
				xfree(inputs);
				return 1;
			}
			time_report_add(&total_time, &file_time);
			time_report_print(inputs[i], &file_time);
		}
		if (redirected)
			restore_stdout(saved);
		total_time.total = tcc_monotonic_seconds() - overall_start;
		time_report_print("total", &total_time);
		free_string_args(link_args, link_arg_count);
		xfree(inputs);
		return 0;
	}

	/* Dump modes: print AST/IR/CFG and stop instead of assembling/linking. */
	if (dump || dump_ir || dump_cfg) {
		int i = 0;
		int use_single_output = (opts.output_file && STRCMP(opts.output_file, "-") != 0);

		if (use_single_output && input_count != 1) {
			tcc_error("-o with dump mode requires exactly one input file\n");
			free_string_args(link_args, link_arg_count);
			xfree(inputs);
			return 1;
		}

		for (i = 0; i < input_count; i++) {
			TimeReport file_time = {0};
			int saved = -1;

			if (i > 0)
				preprocess_reset_file_macros();

			if (use_single_output) {
				saved = redirect_stdout_to(opts.output_file);
				if (saved < 0) {
					free_string_args(link_args, link_arg_count);
					xfree(inputs);
					return 1;
				}
			}

			trace_cc_phase(inputs[i], use_single_output ? opts.output_file : NULL,
			               &opts, cg, requested_dialect);

			if (!codegen_one(inputs[i], cg, &opts, dump, dump_ir, dump_cfg,
			                 require_ir, trace_codegen, strict_ir_except_structs,
			                 &file_time)) {
				if (use_single_output)
					restore_stdout(saved);
				free_string_args(link_args, link_arg_count);
				xfree(inputs);
				return 1;
			}

			if (use_single_output)
				restore_stdout(saved);

			time_report_add(&total_time, &file_time);
			time_report_print(inputs[i], &file_time);
		}

		total_time.total = tcc_monotonic_seconds() - overall_start;
		time_report_print("total", &total_time);
		free_string_args(link_args, link_arg_count);
		xfree(inputs);
		return 0;
	}

	/* -S: emit assembly for each input file.  Match common compiler
	 * behaviour: without -o, write <input-stem>.s even for a single input.
	 * Use -o - explicitly to request stdout. */
	if (opts.emit_asm_only) {
		int i = 0;
		for (i = 0; i < input_count; i++) {
			TimeReport file_time = {0};
			if (i > 0) preprocess_reset_file_macros();
			const char *out_path = NULL;
			char *generated = NULL;

			if (opts.output_file) {
				if (STRCMP(opts.output_file, "-") != 0)
					out_path = opts.output_file;
			} else {
				generated = default_asm_output_name(inputs[i]);
				out_path = generated;
			}

			int saved = -1;
			if (out_path) {
				saved = redirect_stdout_to(out_path);
				if (saved < 0) { xfree(inputs); xfree(generated); return 1; }
			}

			trace_cc_phase(inputs[i], out_path, &opts, cg, requested_dialect);

			int ok = codegen_one(inputs[i], cg, &opts, dump, dump_ir, dump_cfg,
			                     require_ir, trace_codegen, strict_ir_except_structs,
			                     &file_time);

			if (out_path)
				restore_stdout(saved);

	if (out_path && cg == &arm64_codegen &&
	    (!opts.bootstrap_includes ||
	     arm64_bootstrap_peephole_enabled()))
		arm64_peephole_optimize_file(out_path);

			xfree(generated);
			if (!ok) { xfree(inputs); return 1; }
			time_report_add(&total_time, &file_time);
			time_report_print(inputs[i], &file_time);
		}
		total_time.total = tcc_monotonic_seconds() - overall_start;
		time_report_print("total", &total_time);
		free_string_args(link_args, link_arg_count);
		xfree(inputs);
		return 0;
	}

	/* -c: compile each file to an object file */
	if (opts.compile_only) {
		DriverContext ctx = {
			.cg = cg,
			.opts = &opts,
			.requested_dialect = requested_dialect,
			.dump = dump,
			.dump_ir = dump_ir,
			.dump_cfg = dump_cfg,
			.require_ir = require_ir,
			.trace_codegen = trace_codegen,
			.strict_ir_except_structs = strict_ir_except_structs,
			.inputs = inputs,
			.input_count = input_count,
			.link_args = link_args,
			.link_arg_count = link_arg_count,
			.total_time = &total_time,
			.overall_start = overall_start,
		};
		return compile_only_inputs(&ctx);
	}

	/* Full compile+link: compile each file to a temp .o, then link them all */
	{
		DriverContext ctx = {
			.cg = cg,
			.opts = &opts,
			.requested_dialect = requested_dialect,
			.dump = dump,
			.dump_ir = dump_ir,
			.dump_cfg = dump_cfg,
			.require_ir = require_ir,
			.trace_codegen = trace_codegen,
			.strict_ir_except_structs = strict_ir_except_structs,
			.inputs = inputs,
			.input_count = input_count,
			.link_args = link_args,
			.link_arg_count = link_arg_count,
			.total_time = &total_time,
			.overall_start = overall_start,
		};
		return compile_and_link_inputs(&ctx);
	}
}

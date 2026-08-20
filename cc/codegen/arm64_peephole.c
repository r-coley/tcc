#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tcc.h"
#include "arm64_peephole.h"

#undef xmalloc
#undef xcalloc
#undef xrealloc
#undef xfree
#define xmalloc(size)        xmalloc_impl((size), "cc/codegen/arm64_peephole.c", __LINE__, "arm64_peephole")
#define xcalloc(count, size) xcalloc_impl((count), (size), "cc/codegen/arm64_peephole.c", __LINE__, "arm64_peephole")
#define xrealloc(ptr, size)  xrealloc_impl((ptr), (size), "cc/codegen/arm64_peephole.c", __LINE__, "arm64_peephole")
#define xfree(ptr)           xfree_impl((ptr), "cc/codegen/arm64_peephole.c", __LINE__, "arm64_peephole")

static int g_arm64_peephole_depth;

int
arm64_bootstrap_peephole_enabled(void)
{
	const char *env = getenv("TCC_ARM64_BOOT_PEEPHOLE");

	if (!env || !env[0])
		return 1;
	return env[0] != '0';
}

static int
arm64_parse_env_positive_int(const char *name, int *out)
{
	const char *env;
	int value = 0;

	env = getenv(name);
	if (!env || !env[0])
		return 0;
	while (*env) {
		if (*env < '0' || *env > '9')
			return 0;
		value = value * 10 + (*env - '0');
		env++;
	}
	*out = value;
	return 1;
}

static int
arm64_peephole_rule_enabled(int rule_id)
{
	static int initialized = 0;
	static int min_rule = 1;
	static int max_rule = 0x7fffffff;

	if (!initialized) {
		arm64_parse_env_positive_int("TCC_ARM64_PEEPHOLE_MIN_RULE", &min_rule);
		arm64_parse_env_positive_int("TCC_ARM64_PEEPHOLE_MAX_RULE", &max_rule);
		initialized = 1;
	}
	if (rule_id < min_rule)
		return 0;
	return rule_id <= max_rule;
}

static int
arm64_peephole_max_passes(void)
{
	static int initialized = 0;
	static int max_passes = 2;

	if (!initialized) {
		arm64_parse_env_positive_int("TCC_ARM64_PEEPHOLE_MAX_PASSES",
		                              &max_passes);
		if (max_passes < 1)
			max_passes = 1;
		initialized = 1;
	}
	return max_passes;
}

static int
arm64_parse_mov_reg_imm(const char *line, const char *reg, unsigned long *imm_out)
{
	const char *prefix0 = "    movz ";
	const char *prefix1 = "    mov ";
	const char *num;
	unsigned long value = 0;
	int base = 10;
	size_t reg_len;

	if (!line || !reg || !imm_out)
		return 0;
	reg_len = strlen(reg);
	if (strncmp(line, prefix0, strlen(prefix0)) == 0) {
		num = line + strlen(prefix0);
	} else if (strncmp(line, prefix1, strlen(prefix1)) == 0) {
		num = line + strlen(prefix1);
	} else {
		return 0;
	}
	if (strncmp(num, reg, reg_len) != 0 ||
	    strncmp(num + reg_len, ", #", 3) != 0)
		return 0;
	num += reg_len + 3;
	if (!*num)
		return 0;
	if (num[0] == '0' && (num[1] == 'x' || num[1] == 'X')) {
		base = 16;
		num += 2;
		if (!*num)
			return 0;
	}
	while (*num) {
		unsigned long digit;

		if (*num >= '0' && *num <= '9')
			digit = (unsigned long)(*num - '0');
		else if (base == 16 && *num >= 'a' && *num <= 'f')
			digit = 10u + (unsigned long)(*num - 'a');
		else if (base == 16 && *num >= 'A' && *num <= 'F')
			digit = 10u + (unsigned long)(*num - 'A');
		else
			return 0;
		value = value * (unsigned long)base + digit;
		num++;
	}
	*imm_out = value;
	return 1;
}

static int
arm64_parse_mov_x0_imm(const char *line, unsigned long *imm_out)
{
	return arm64_parse_mov_reg_imm(line, "x0", imm_out);
}

static int
arm64_parse_mov_x9_imm(const char *line, unsigned long *imm_out)
{
	return arm64_parse_mov_reg_imm(line, "x9", imm_out);
}

static int
arm64_is_store_using_w9_only(const char *line)
{
	if (!line)
		return 0;
	return strncmp(line, "    str w9, [", 13) == 0;
}

static int
arm64_parse_small_signed_x0_imm(const char *line, long *imm_out)
{
	unsigned long imm;

	if (!line || !imm_out)
		return 0;
	if (arm64_parse_mov_x0_imm(line, &imm)) {
		*imm_out = (long)imm;
		return 1;
	}
	if (STRCMP(line, "    mov x0, #-0x1") == 0 ||
	    STRCMP(line, "    mov x0, #-1") == 0 ||
	    STRCMP(line, "    movn x0, #0") == 0) {
		*imm_out = -1;
		return 1;
	}
	return 0;
}

static int
arm64_parse_and_x0_x0_imm(const char *line, unsigned long *imm_out)
{
	const char *prefix = "    and x0, x0, #";
	const char *num;
	unsigned long value = 0;

	if (!line || !imm_out)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	num = line + strlen(prefix);
	if (!*num)
		return 0;
	while (*num) {
		if (*num < '0' || *num > '9')
			return 0;
		value = value * 10u + (unsigned long)(*num - '0');
		num++;
	}
	*imm_out = value;
	return 1;
}

static int
arm64_parse_add_x0_x0_imm(const char *line, unsigned long *imm_out)
{
	const char *prefix0 = "    add x0, x0, #";
	const char *prefix1 = "    add  x0, x0, #";
	const char *num;
	int base = 10;
	unsigned long value = 0;

	if (!line || !imm_out)
		return 0;
	if (strncmp(line, prefix0, strlen(prefix0)) == 0)
		num = line + strlen(prefix0);
	else if (strncmp(line, prefix1, strlen(prefix1)) == 0)
		num = line + strlen(prefix1);
	else
		return 0;
	if (!*num)
		return 0;
	if (num[0] == '0' && (num[1] == 'x' || num[1] == 'X')) {
		base = 16;
		num += 2;
		if (!*num)
			return 0;
	}
	while (*num) {
		unsigned long digit;

		if (*num >= '0' && *num <= '9')
			digit = (unsigned long)(*num - '0');
		else if (base == 16 && *num >= 'a' && *num <= 'f')
			digit = 10u + (unsigned long)(*num - 'a');
		else if (base == 16 && *num >= 'A' && *num <= 'F')
			digit = 10u + (unsigned long)(*num - 'A');
		else
			return 0;
		value = value * (unsigned long)base + digit;
		num++;
	}
	*imm_out = value;
	return 1;
}

static int
arm64_parse_sub_reg_reg_imm(const char *line, const char *dst, const char *src,
			    unsigned long *imm_out)
{
	char prefix[64];
	const char *num;
	unsigned long value = 0;

	if (!line || !dst || !src || !imm_out)
		return 0;
	snprintf(prefix, sizeof(prefix), "    sub %s, %s, #", dst, src);
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	num = line + strlen(prefix);
	if (!*num)
		return 0;
	while (*num) {
		if (*num < '0' || *num > '9')
			return 0;
		value = value * 10u + (unsigned long)(*num - '0');
		num++;
	}
	*imm_out = value;
	return 1;
}

static int
arm64_parse_frame_negative_offset(const char *line, const char *op,
				  unsigned long *offset_out)
{
	char prefix[64];
	const char *num;
	unsigned long value = 0;

	if (!line || !op || !offset_out)
		return 0;
	snprintf(prefix, sizeof(prefix), "    %s, [x29, #-", op);
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	num = line + strlen(prefix);
	if (!*num)
		return 0;
	while (*num) {
		if (*num == ']') {
			*offset_out = value;
			return num[1] == '\0';
		}
		if (*num < '0' || *num > '9')
			return 0;
		value = value * 10u + (unsigned long)(*num - '0');
		num++;
	}
	return 0;
}

static int
arm64_parse_mov_arg_x0(const char *line, char *reg_out, size_t reg_out_size)
{
	const char *prefix = "    mov x";

	if (!line || !reg_out || reg_out_size < 3)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	if (line[9] < '1' || line[9] > '7')
		return 0;
	if (STRCMP(line + 10, ", x0") != 0)
		return 0;
	reg_out[0] = 'x';
	reg_out[1] = line[9];
	reg_out[2] = '\0';
	return 1;
}

static int
arm64_parse_adrp_ldr_got_x0(const char *adrp_line, const char *ldr_line,
			    char *symbol_out, size_t symbol_out_size)
{
	const char *adrp_prefix = "    adrp x0, ";
	const char *ldr_prefix0 = "    ldr x0, [x0, ";
	const char *ldr_prefix1 = "    ldr  x0, [x0, ";
	const char *got_suffix = "@GOTPAGE";
	const char *gotoff_suffix = "@GOTPAGEOFF]";
	const char *symbol;
	const char *got;
	const char *ldr_symbol;
	size_t symbol_len;

	if (!adrp_line || !ldr_line || !symbol_out || symbol_out_size == 0)
		return 0;
	if (strncmp(adrp_line, adrp_prefix, strlen(adrp_prefix)) != 0)
		return 0;
	symbol = adrp_line + strlen(adrp_prefix);
	got = strstr(symbol, got_suffix);
	if (!got || STRCMP(got, got_suffix) != 0)
		return 0;
	symbol_len = (size_t)(got - symbol);
	if (symbol_len == 0 || symbol_len + 1 > symbol_out_size)
		return 0;
	if (strncmp(ldr_line, ldr_prefix0, strlen(ldr_prefix0)) == 0)
		ldr_symbol = ldr_line + strlen(ldr_prefix0);
	else if (strncmp(ldr_line, ldr_prefix1, strlen(ldr_prefix1)) == 0)
		ldr_symbol = ldr_line + strlen(ldr_prefix1);
	else
		return 0;
	if (strncmp(ldr_symbol, symbol, symbol_len) != 0)
		return 0;
	if (STRCMP(ldr_symbol + symbol_len, gotoff_suffix) != 0)
		return 0;
	memcpy(symbol_out, symbol, symbol_len);
	symbol_out[symbol_len] = '\0';
	return 1;
}

static int
arm64_parse_adrp_ldr_got_x1(const char *adrp_line, const char *ldr_line,
			    char *symbol_out, size_t symbol_out_size)
{
	const char *adrp_prefix = "    adrp x1, ";
	const char *ldr_prefix0 = "    ldr x1, [x1, ";
	const char *ldr_prefix1 = "    ldr  x1, [x1, ";
	const char *got_suffix = "@GOTPAGE";
	const char *gotoff_suffix = "@GOTPAGEOFF]";
	const char *symbol;
	const char *got;
	const char *ldr_symbol;
	size_t symbol_len;

	if (!adrp_line || !ldr_line || !symbol_out || symbol_out_size == 0)
		return 0;
	if (strncmp(adrp_line, adrp_prefix, strlen(adrp_prefix)) != 0)
		return 0;
	symbol = adrp_line + strlen(adrp_prefix);
	got = strstr(symbol, got_suffix);
	if (!got || STRCMP(got, got_suffix) != 0)
		return 0;
	symbol_len = (size_t)(got - symbol);
	if (symbol_len == 0 || symbol_len + 1 > symbol_out_size)
		return 0;
	if (strncmp(ldr_line, ldr_prefix0, strlen(ldr_prefix0)) == 0)
		ldr_symbol = ldr_line + strlen(ldr_prefix0);
	else if (strncmp(ldr_line, ldr_prefix1, strlen(ldr_prefix1)) == 0)
		ldr_symbol = ldr_line + strlen(ldr_prefix1);
	else
		return 0;
	if (strncmp(ldr_symbol, symbol, symbol_len) != 0)
		return 0;
	if (STRCMP(ldr_symbol + symbol_len, gotoff_suffix) != 0)
		return 0;
	memcpy(symbol_out, symbol, symbol_len);
	symbol_out[symbol_len] = '\0';
	return 1;
}

static int
arm64_parse_adrp_add_page_x0(const char *adrp_line, const char *add_line,
			     char *symbol_out, size_t symbol_out_size)
{
	const char *adrp_prefix = "    adrp x0, ";
	const char *add_prefix0 = "    add x0, x0, ";
	const char *add_prefix1 = "    add  x0, x0, ";
	const char *page_suffix = "@PAGE";
	const char *pageoff_suffix = "@PAGEOFF";
	const char *symbol;
	const char *page;
	const char *add_symbol;
	size_t symbol_len;

	if (!adrp_line || !add_line || !symbol_out || symbol_out_size == 0)
		return 0;
	if (strncmp(adrp_line, adrp_prefix, strlen(adrp_prefix)) != 0)
		return 0;
	symbol = adrp_line + strlen(adrp_prefix);
	page = strstr(symbol, page_suffix);
	if (!page || STRCMP(page, page_suffix) != 0)
		return 0;
	symbol_len = (size_t)(page - symbol);
	if (symbol_len == 0 || symbol_len + 1 > symbol_out_size)
		return 0;
	if (strncmp(add_line, add_prefix0, strlen(add_prefix0)) == 0)
		add_symbol = add_line + strlen(add_prefix0);
	else if (strncmp(add_line, add_prefix1, strlen(add_prefix1)) == 0)
		add_symbol = add_line + strlen(add_prefix1);
	else
		return 0;
	if (strncmp(add_symbol, symbol, symbol_len) != 0)
		return 0;
	if (STRCMP(add_symbol + symbol_len, pageoff_suffix) != 0)
		return 0;
	memcpy(symbol_out, symbol, symbol_len);
	symbol_out[symbol_len] = '\0';
	return 1;
}

static int
arm64_parse_adrp_page_x1(const char *line, char *symbol_out, size_t symbol_out_size)
{
	const char *prefix = "    adrp x1, ";
	const char *page_suffix = "@PAGE";
	const char *symbol;
	const char *page;
	size_t symbol_len;

	if (!line || !symbol_out || symbol_out_size == 0)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	symbol = line + strlen(prefix);
	page = strstr(symbol, page_suffix);
	if (!page || STRCMP(page, page_suffix) != 0)
		return 0;
	symbol_len = (size_t)(page - symbol);
	if (symbol_len == 0 || symbol_len + 1 > symbol_out_size)
		return 0;
	memcpy(symbol_out, symbol, symbol_len);
	symbol_out[symbol_len] = '\0';
	return 1;
}

static int
arm64_parse_add_pageoff_x1(const char *line, const char *symbol)
{
	const char *prefix0 = "    add x1, x1, ";
	const char *prefix1 = "    add  x1, x1, ";
	const char *suffix = "@PAGEOFF";
	size_t symbol_len;

	if (!line || !symbol)
		return 0;
	if (strncmp(line, prefix0, strlen(prefix0)) == 0)
		line += strlen(prefix0);
	else if (strncmp(line, prefix1, strlen(prefix1)) == 0)
		line += strlen(prefix1);
	else
		return 0;
	symbol_len = strlen(symbol);
	if (strncmp(line, symbol, symbol_len) != 0)
		return 0;
	return STRCMP(line + symbol_len, suffix) == 0;
}

static int
arm64_parse_ldrsw_pageoff_x1(const char *line, const char *symbol)
{
	const char *prefix = "    ldrsw x0, [x1, ";
	const char *suffix = "@PAGEOFF]";
	size_t symbol_len;

	if (!line || !symbol)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	line += strlen(prefix);
	symbol_len = strlen(symbol);
	if (strncmp(line, symbol, symbol_len) != 0)
		return 0;
	return STRCMP(line + symbol_len, suffix) == 0;
}

static int
arm64_parse_ldr_w8_pageoff_x1(const char *line, const char *symbol)
{
	const char *prefix = "    ldr w8, [x1, ";
	const char *suffix = "@PAGEOFF]";
	size_t symbol_len;

	if (!line || !symbol)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	line += strlen(prefix);
	symbol_len = strlen(symbol);
	if (strncmp(line, symbol, symbol_len) != 0)
		return 0;
	return STRCMP(line + symbol_len, suffix) == 0;
}

static int
arm64_parse_str_w0_pageoff_x1(const char *line, const char *symbol)
{
	const char *prefix = "    str w0, [x1, ";
	const char *suffix = "@PAGEOFF]";
	size_t symbol_len;

	if (!line || !symbol)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	line += strlen(prefix);
	symbol_len = strlen(symbol);
	if (strncmp(line, symbol, symbol_len) != 0)
		return 0;
	return STRCMP(line + symbol_len, suffix) == 0;
}

static int
arm64_is_lowbit_mask_imm(unsigned long imm)
{
	if (imm == 0)
		return 1;
	if ((imm & (imm + 1ul)) == 0)
		return 1;
	return (imm & (imm - 1ul)) == 0;
}

static int
arm64_parse_power2_shift(unsigned long imm, unsigned long *shift_out)
{
	unsigned long shift = 0;

	if (!imm || !shift_out || (imm & (imm - 1ul)) != 0)
		return 0;
	while (imm > 1) {
		imm >>= 1;
		shift++;
	}
	*shift_out = shift;
	return 1;
}

static int
arm64_emit_direct_zero_store(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    str x0, [x1", 15) == 0) {
		rest = line + 15;
		fprintf(out, "    str xzr, [x0%s\n", rest);
		return 1;
	}

	if (strncmp(line, "    str w0, [x1", 15) == 0) {
		rest = line + 15;
		fprintf(out, "    str wzr, [x0%s\n", rest);
		return 1;
	}

	if (strncmp(line, "    strb w0, [x1", 16) == 0) {
		rest = line + 16;
		fprintf(out, "    strb wzr, [x0%s\n", rest);
		return 1;
	}

	if (strncmp(line, "    strh w0, [x1", 16) == 0) {
		rest = line + 16;
		fprintf(out, "    strh wzr, [x0%s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_is_zero_mov_x0(const char *line)
{
	if (!line)
		return 0;
	if (STRCMP(line, "    movz x0, #0") == 0)
		return 1;
	if (STRCMP(line, "    mov x0, #0") == 0)
		return 1;
	if (STRCMP(line, "    mov x0, #0x0") == 0)
		return 1;
	return 0;
}

static int
arm64_emit_x1_load_equivalent(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    ldur x0, [x29", 17) == 0) {
		rest = line + 13;
		fprintf(out, "    ldur x1, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    ldr x0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr x1, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    ldr x0, [sp", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr x1, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_xreg_load_equivalent(FILE *out, const char *reg, const char *line)
{
	const char *rest;

	if (!out || !reg || !line)
		return 0;

	if (strncmp(line, "    ldur x0, [x29", 17) == 0) {
		rest = line + 13;
		fprintf(out, "    ldur %s, %s\n", reg, rest);
		return 1;
	}

	if (strncmp(line, "    ldr x0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr %s, %s\n", reg, rest);
		return 1;
	}

	if (strncmp(line, "    ldr x0, [sp", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr %s, %s\n", reg, rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_xreg_signed_load_equivalent(FILE *out, const char *reg,
				       const char *line)
{
	const char *rest;

	if (!out || !reg || !line)
		return 0;

	if (strncmp(line, "    ldursw x0, ", 15) == 0) {
		rest = line + 15;
		fprintf(out, "    ldursw %s, %s\n", reg, rest);
		return 1;
	}

	if (strncmp(line, "    ldrsw x0, ", 14) == 0) {
		rest = line + 14;
		fprintf(out, "    ldrsw %s, %s\n", reg, rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_xreg_value_equivalent(FILE *out, const char *reg, const char *line)
{
	const char *rest;

	if (!out || !reg || !line)
		return 0;
	if (arm64_emit_xreg_load_equivalent(out, reg, line))
		return 1;

	if (strncmp(line, "    sub x0, x29, #", 18) == 0) {
		rest = line + 12;
		fprintf(out, "    sub %s, %s\n", reg, rest);
		return 1;
	}

	if (strncmp(line, "    add x0, x29, #", 18) == 0) {
		rest = line + 12;
		fprintf(out, "    add %s, %s\n", reg, rest);
		return 1;
	}

	if (strncmp(line, "    add x0, sp, #", 17) == 0) {
		rest = line + 12;
		fprintf(out, "    add %s, %s\n", reg, rest);
		return 1;
	}

	if (strncmp(line, "    mov x0, x", 13) == 0) {
		rest = line + 12;
		fprintf(out, "    mov %s, %s\n", reg, rest);
		return 1;
	}

	return 0;
}

static int
arm64_is_x0_value_equivalent_line(const char *line)
{
	if (!line)
		return 0;
	return strncmp(line, "    ldur x0, [x29", 17) == 0 ||
	       strncmp(line, "    ldr x0, [x29", 16) == 0 ||
	       strncmp(line, "    ldr x0, [sp", 15) == 0 ||
	       strncmp(line, "    sub x0, x29, #", 18) == 0 ||
	       strncmp(line, "    add x0, x29, #", 18) == 0 ||
	       strncmp(line, "    add x0, sp, #", 17) == 0 ||
	       strncmp(line, "    mov x0, x", 13) == 0;
}

static int
arm64_is_add_sp_sp_48(const char *line)
{
	if (!line)
		return 0;
	return STRCMP(line, "    add sp, sp, #48") == 0 ||
	       STRCMP(line, "    add sp, sp, #0x30") == 0;
}

static int
arm64_is_push_x0_sp16(const char *line)
{
	if (!line)
		return 0;
	return STRCMP(line, "    str x0, [sp, #-16]!") == 0 ||
	       STRCMP(line, "    str x0, [sp, #-0x10]!") == 0;
}

static int
arm64_is_ldr_xreg_sp_offset(const char *line, const char *reg, int offset)
{
	char dec_buf[64];
	char hex_buf[64];

	if (!line || !reg)
		return 0;
	if (offset == 0) {
		snprintf(dec_buf, sizeof(dec_buf), "    ldr %s, [sp]", reg);
		if (STRCMP(line, dec_buf) == 0)
			return 1;
		snprintf(dec_buf, sizeof(dec_buf), "    ldr %s, [sp, #0]", reg);
		if (STRCMP(line, dec_buf) == 0)
			return 1;
		snprintf(hex_buf, sizeof(hex_buf), "    ldr %s, [sp, #0x0]", reg);
		return STRCMP(line, hex_buf) == 0;
	}
	snprintf(dec_buf, sizeof(dec_buf), "    ldr %s, [sp, #%d]", reg, offset);
	if (STRCMP(line, dec_buf) == 0)
		return 1;
	snprintf(hex_buf, sizeof(hex_buf), "    ldr %s, [sp, #0x%x]", reg, offset);
	return STRCMP(line, hex_buf) == 0;
}

static int
arm64_emit_w0_store_equivalent(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    ldursw x0, [x29", 19) == 0) {
		rest = line + 15;
		fprintf(out, "    stur w0, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    ldrsw x0, [x29", 18) == 0) {
		rest = line + 14;
		fprintf(out, "    str w0, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    ldr w0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    str w0, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_zero_frame_store(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    str x0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    str xzr, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    stur x0, [x29", 17) == 0) {
		rest = line + 13;
		fprintf(out, "    stur xzr, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    str x0, [sp", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    str xzr, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    str w0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    str wzr, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    stur w0, [x29", 17) == 0) {
		rest = line + 13;
		fprintf(out, "    stur wzr, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    str w0, [sp", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    str wzr, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_x17_load_equivalent(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    ldur x0, [x29", 17) == 0) {
		rest = line + 13;
		fprintf(out, "    ldur x17, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    ldr x0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr x17, %s\n", rest);
		return 1;
	}

	if (strncmp(line, "    ldr x0, [sp", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr x17, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_x1_deref_from_x1_equivalent(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    ldr x0, [x0", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr x1, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_w1_deref_from_x1_equivalent(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    ldr w0, [x0", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr w1, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_emit_w1_int_value_equivalent(FILE *out, const char *line)
{
	const char *rest;

	if (!out || !line)
		return 0;

	if (strncmp(line, "    ldr w0, [x29", 16) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr w1, %s\n", rest);
		return 1;
	}
	if (strncmp(line, "    ldur w0, [x29", 17) == 0) {
		rest = line + 13;
		fprintf(out, "    ldur w1, %s\n", rest);
		return 1;
	}
	if (strncmp(line, "    ldrsw x0, [x29", 18) == 0) {
		rest = line + 14;
		fprintf(out, "    ldrsw x1, %s\n", rest);
		return 1;
	}
	if (strncmp(line, "    ldursw x0, [x29", 19) == 0) {
		rest = line + 15;
		fprintf(out, "    ldursw x1, %s\n", rest);
		return 1;
	}
	if (strncmp(line, "    mov x0, #", 13) == 0) {
		rest = line + 12;
		fprintf(out, "    mov w1, %s\n", rest);
		return 1;
	}
	if (strncmp(line, "    movz x0, #", 14) == 0) {
		rest = line + 13;
		fprintf(out, "    movz w1, %s\n", rest);
		return 1;
	}

	return 0;
}

static int
arm64_is_x0_int_value_equivalent_line(const char *line)
{
	return line &&
	       (strncmp(line, "    ldrsw x0, [x29", 18) == 0 ||
		strncmp(line, "    ldursw x0, [x29", 19) == 0 ||
		strncmp(line, "    ldr w0, [x29", 16) == 0 ||
		strncmp(line, "    ldur w0, [x29", 17) == 0 ||
		strncmp(line, "    ldrsw x0, [sp", 17) == 0 ||
		strncmp(line, "    ldr w0, [sp", 15) == 0 ||
		strncmp(line, "    mov x0, #", 13) == 0 ||
		strncmp(line, "    movz x0, #", 14) == 0);
}

static int
arm64_is_redundant_x9_stack_self_store(const char *load, const char *store)
{
	const char *load_addr;
	const char *store_addr;

	if (!load || !store)
		return 0;
	if (strncmp(load, "    ldr x9, [sp", 15) != 0 ||
	    strncmp(store, "    str x9, [sp", 15) != 0)
		return 0;
	load_addr = load + 12;
	store_addr = store + 12;
	return STRCMP(load_addr, store_addr) == 0;
}

static int
arm64_is_loc_line(const char *line)
{
	if (!line)
		return 0;
	return strncmp(line, "    .loc ", 9) == 0;
}

static int
arm64_is_instruction_line(const char *line)
{
	if (!line)
		return 0;
	return strncmp(line, "    ", 4) == 0 &&
	       line[4] != '\0' &&
	       line[4] != '.';
}

static int
arm64_can_omit_frame_restore(char **lines, int index)
{
	int j;

	if (!lines || index < 1)
		return 0;

	for (j = index - 1; j >= 0; j--) {
		if (STRCMP(lines[j], "    mov x29, sp") == 0)
			break;
		if (arm64_is_instruction_line(lines[j]) && strstr(lines[j], "sp"))
			return 0;
	}
	if (j < 1)
		return 0;

	j--;
	while (j >= 0 && arm64_is_loc_line(lines[j]))
		j--;
	return j >= 0 && STRCMP(lines[j], "    stp x29, x30, [sp, #-16]!") == 0;
}

static int
arm64_is_large_frame_base_x9(const char *line)
{
	if (!line)
		return 0;
	return strncmp(line, "    sub x9, x29, #", 18) == 0 &&
	       strstr(line, ", lsl #12") != NULL;
}

static int
arm64_is_plain_x9_mem_access(const char *line)
{
	if (!line || !arm64_is_instruction_line(line))
		return 0;
	if (!strstr(line, "[x9]") && !strstr(line, "[x9,"))
		return 0;
	if (strstr(line, "]!"))
		return 0;
	return strncmp(line, "    ldr", 7) == 0 ||
	       strncmp(line, "    ldur", 8) == 0 ||
	       strncmp(line, "    ldp", 7) == 0 ||
	       strncmp(line, "    str", 7) == 0 ||
	       strncmp(line, "    stur", 8) == 0 ||
	       strncmp(line, "    stp", 7) == 0;
}

static int
arm64_emit_xreg_from_x0_x9_load(FILE *out, const char *reg, const char *line)
{
	char wreg[8];
	const char *rest;

	if (!out || !reg || !line)
		return 0;
	if (reg[0] == 'x' && reg[1] != '\0' && reg[2] == '\0')
		snprintf(wreg, sizeof(wreg), "w%c", reg[1]);
	else
		snprintf(wreg, sizeof(wreg), "%s", reg);
	if (strncmp(line, "    ldrsw x0, [x9", 17) == 0) {
		rest = line + 14;
		fprintf(out, "    ldrsw %s, %s\n", reg, rest);
		return 1;
	}
	if (strncmp(line, "    ldr x0, [x9", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr %s, %s\n", reg, rest);
		return 1;
	}
	if (strncmp(line, "    ldr w0, [x9", 15) == 0) {
		rest = line + 12;
		fprintf(out, "    ldr %s, %s\n", wreg, rest);
		return 1;
	}
	if (strncmp(line, "    ldrb w0, [x9", 16) == 0) {
		rest = line + 13;
		fprintf(out, "    ldrb %s, %s\n", wreg, rest);
		return 1;
	}
	if (strncmp(line, "    ldrh w0, [x9", 16) == 0) {
		rest = line + 13;
		fprintf(out, "    ldrh %s, %s\n", wreg, rest);
		return 1;
	}
	return 0;
}

static int
arm64_emit_x1_from_x0_x9_load(FILE *out, const char *line)
{
	return arm64_emit_xreg_from_x0_x9_load(out, "x1", line);
}

static int
arm64_is_x0_x9_load(const char *line)
{
	return line &&
	       (strncmp(line, "    ldrsw x0, [x9", 17) == 0 ||
		strncmp(line, "    ldr x0, [x9", 15) == 0 ||
		strncmp(line, "    ldr w0, [x9", 15) == 0 ||
		strncmp(line, "    ldrb w0, [x9", 16) == 0 ||
		strncmp(line, "    ldrh w0, [x9", 16) == 0);
}

static int
arm64_is_x0_non_x9_compare_load(const char *line)
{
	if (!line || strstr(line, "[x9"))
		return 0;
	return strncmp(line, "    ldrsw x0, [x29", 18) == 0 ||
	       strncmp(line, "    ldursw x0, [x29", 19) == 0 ||
	       strncmp(line, "    ldr w0, [x29", 16) == 0 ||
	       strncmp(line, "    ldur w0, [x29", 17) == 0 ||
	       strncmp(line, "    ldr x0, [x29", 16) == 0 ||
	       strncmp(line, "    ldur x0, [x29", 17) == 0;
}

static int
arm64_is_x0_local_int_load(const char *line)
{
	return line &&
	       (strncmp(line, "    ldrsw x0, [x29", 18) == 0 ||
		strncmp(line, "    ldursw x0, [x29", 19) == 0 ||
		strncmp(line, "    ldr w0, [x29", 16) == 0 ||
		strncmp(line, "    ldur w0, [x29", 17) == 0 ||
		strncmp(line, "    ldrsw x0, [sp", 17) == 0 ||
		strncmp(line, "    ldr w0, [sp", 15) == 0);
}

static int
arm64_is_safe_between_x9_base_reuse(const char *line)
{
	if (!line || !arm64_is_instruction_line(line))
		return 0;
	if (strstr(line, "x9") || strstr(line, "w9"))
		return 0;
	if (strncmp(line, "    b", 5) == 0 ||
	    strncmp(line, "    cb", 6) == 0 ||
	    strncmp(line, "    tb", 6) == 0 ||
	    strncmp(line, "    ret", 7) == 0 ||
	    strncmp(line, "    bl", 6) == 0)
		return 0;
	return 1;
}

static int
arm64_parse_unconditional_branch_label(const char *line, char *label_out, size_t label_out_size)
{
	const char *prefix = "    b ";
	size_t len;

	if (!line || !label_out || label_out_size == 0)
		return 0;
	if (strncmp(line, prefix, strlen(prefix)) != 0)
		return 0;
	line += strlen(prefix);
	if (line[0] != 'L')
		return 0;
	len = strlen(line);
	if (len == 0 || len + 1 > label_out_size)
		return 0;
	memcpy(label_out, line, len + 1);
	return 1;
}

static int
arm64_parse_local_label_name(const char *line, char *label_out, size_t label_out_size)
{
	size_t len;

	if (!line || !label_out || label_out_size == 0 || line[0] != 'L')
		return 0;
	len = strlen(line);
	if (len < 2 || line[len - 1] != ':' || len > label_out_size)
		return 0;
	memcpy(label_out, line, len - 1);
	label_out[len - 1] = '\0';
	return 1;
}

static int
arm64_parse_zero_store_x17(const char *line, int *is64_out, unsigned long *offset_out)
{
	const char *prefix64 = "    str xzr, [x17";
	const char *prefix32 = "    str wzr, [x17";
	const char *rest;
	unsigned long offset = 0;

	if (!line || !is64_out || !offset_out)
		return 0;

	if (strncmp(line, prefix64, strlen(prefix64)) == 0) {
		*is64_out = 1;
		rest = line + strlen(prefix64);
	} else if (strncmp(line, prefix32, strlen(prefix32)) == 0) {
		*is64_out = 0;
		rest = line + strlen(prefix32);
	} else {
		return 0;
	}

	if (STRCMP(rest, "]") == 0) {
		*offset_out = 0;
		return 1;
	}
	if (strncmp(rest, ", #", 3) != 0)
		return 0;
	rest += 3;
	if (!*rest)
		return 0;
	while (*rest >= '0' && *rest <= '9') {
		offset = offset * 10u + (unsigned long)(*rest - '0');
		rest++;
	}
	if (STRCMP(rest, "]") != 0)
		return 0;
	*offset_out = offset;
	return 1;
}

static int
arm64_store_unsigned_offset_encodable(int is64, unsigned long offset)
{
	if (is64)
		return offset <= 32760u && (offset & 7u) == 0;
	return offset <= 16380u && (offset & 3u) == 0;
}

static void
arm64_emit_zero_store_x17(FILE *out, int is64, unsigned long offset)
{
	if (is64)
		fprintf(out, "    str xzr, [x17, #%lu]\n", offset);
	else
		fprintf(out, "    str wzr, [x17, #%lu]\n", offset);
}

static void
arm64_emit_zero_store_x0(FILE *out, int is64, unsigned long offset)
{
	if (is64)
		fprintf(out, "    str xzr, [x0, #%lu]\n", offset);
	else
		fprintf(out, "    str wzr, [x0, #%lu]\n", offset);
}

static int
arm64_x17_dead_before_next_use(char **lines, int count, int start)
{
	for (int i = start; i < count; i++) {
		if (strstr(lines[i], "x17")) {
			if (strncmp(lines[i], "    mov x17, ", 13) == 0 ||
			    strncmp(lines[i], "    movz x17, ", 14) == 0 ||
			    strncmp(lines[i], "    add x17, ", 13) == 0 ||
			    strncmp(lines[i], "    sub x17, ", 13) == 0 ||
			    strncmp(lines[i], "    ldr x17, ", 13) == 0 ||
			    strncmp(lines[i], "    ldur x17, ", 14) == 0 ||
			    strncmp(lines[i], "    adrp x17, ", 14) == 0)
				return 1;
			return 0;
		}
		if (strncmp(lines[i], "    bl ", 7) == 0 ||
		    strncmp(lines[i], "    b ", 6) == 0 ||
		    STRCMP(lines[i], "    ret") == 0)
			return 1;
	}
	return 1;
}

static int
arm64_peephole_try_fold(FILE *out, char **lines, int count, int *index)
{
	unsigned long imm;
	unsigned long imm2;
	unsigned long off0;
	unsigned long off1;
	unsigned long off2;
	long simm;
	char symbol[256];
	char symbol2[256];
	char reg[4];
	int is64_0;
	int is64_1;
	int is64_2;
	int store_count;
	int store_is64[8];
	unsigned long store_offset[8];
	int i;

	if (!out || !lines || !index)
		return 0;

	i = *index;
	if (arm64_peephole_rule_enabled(79) &&
	    i + 9 < count &&
	    (strncmp(lines[i], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i], "    ldr x0, [sp", 15) == 0) &&
	    STRCMP(lines[i + 1], "    ldr x0, [x0, #0]") == 0 &&
	    STRCMP(lines[i + 2], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 3], "    ldursw x0, [x29", 19) == 0 ||
	     strncmp(lines[i + 3], "    ldrsw x0, [x29", 18) == 0) &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    add x0, x1, w0, sxtw #3") == 0 &&
	    STRCMP(lines[i + 6], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 7], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i + 7], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i + 7], "    ldr x0, [sp", 15) == 0) &&
	    STRCMP(lines[i + 8], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 9], "    str x0, [x1, #0]") == 0) {
		if (!arm64_emit_xreg_load_equivalent(out, "x1", lines[i]))
			return 0;
		fprintf(out, "    ldr x1, [x1, #0]\n");
		if (!arm64_emit_xreg_signed_load_equivalent(out, "x2", lines[i + 3]))
			return 0;
		fprintf(out, "%s\n", lines[i + 7]);
		fprintf(out, "    str x0, [x1, x2, lsl #3]\n");
		*index = i + 10;
		return 1;
	}
	if (arm64_peephole_rule_enabled(80) &&
	    i + 19 < count &&
	    (strncmp(lines[i], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i], "    ldr x0, [sp", 15) == 0) &&
	    STRCMP(lines[i + 1], "    scvtf d0, x0") == 0 &&
	    STRCMP(lines[i + 2], "    fmov x0, d0") == 0 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 4], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i + 4], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i + 4], "    ldr x0, [sp", 15) == 0) &&
	    STRCMP(lines[i + 5], "    scvtf d0, x0") == 0 &&
	    STRCMP(lines[i + 6], "    fmov x0, d0") == 0 &&
	    STRCMP(lines[i + 7], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 8], "    movz x0, #", 14) == 0 &&
	    strncmp(lines[i + 9], "    movk x0, #", 14) == 0 &&
	    STRCMP(lines[i + 10], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 11], "    fmov d0, x0") == 0 &&
	    STRCMP(lines[i + 12], "    fmov d1, x1") == 0 &&
	    STRCMP(lines[i + 13], "    fdiv d0, d1, d0") == 0 &&
	    STRCMP(lines[i + 14], "    fmov x0, d0") == 0 &&
	    STRCMP(lines[i + 15], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 16], "    fmov d0, x0") == 0 &&
	    STRCMP(lines[i + 17], "    fmov d1, x1") == 0 &&
	    STRCMP(lines[i + 18], "    fadd d0, d1, d0") == 0 &&
	    STRCMP(lines[i + 19], "    fmov x0, d0") == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    scvtf d0, x0\n");
		if (!arm64_emit_xreg_load_equivalent(out, "x1", lines[i + 4]))
			return 0;
		fprintf(out, "    scvtf d1, x1\n");
		fprintf(out, "%s\n", lines[i + 8]);
		fprintf(out, "%s\n", lines[i + 9]);
		fprintf(out, "    fmov d2, x0\n");
		fprintf(out, "    fdiv d1, d1, d2\n");
		fprintf(out, "    fadd d0, d0, d1\n");
		fprintf(out, "    fmov x0, d0\n");
		*index = i + 20;
		return 1;
	}
	if (arm64_peephole_rule_enabled(81) &&
	    i + 2 < count &&
	    arm64_parse_mov_x9_imm(lines[i], &imm) &&
	    arm64_is_store_using_w9_only(lines[i + 1]) &&
	    arm64_parse_mov_x9_imm(lines[i + 2], &imm2) &&
	    imm == imm2) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 3;
		return 1;
	}
	if (arm64_peephole_rule_enabled(38) &&
	    i + 2 < count &&
	    arm64_is_large_frame_base_x9(lines[i]) &&
	    arm64_is_plain_x9_mem_access(lines[i + 1]) &&
	    STRCMP(lines[i], lines[i + 2]) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 3;
		return 1;
	}
	if (arm64_peephole_rule_enabled(39) &&
	    i + 3 < count &&
	    arm64_is_large_frame_base_x9(lines[i]) &&
	    arm64_is_plain_x9_mem_access(lines[i + 1]) &&
	    arm64_is_safe_between_x9_base_reuse(lines[i + 2]) &&
	    STRCMP(lines[i], lines[i + 3]) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(40) &&
	    i + 5 < count &&
	    arm64_is_large_frame_base_x9(lines[i]) &&
	    arm64_is_x0_x9_load(lines[i + 1]) &&
	    arm64_is_push_x0_sp16(lines[i + 2]) &&
	    arm64_is_plain_x9_mem_access(lines[i + 3]) &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    (STRCMP(lines[i + 5], "    cmp w1, w0") == 0 ||
	     STRCMP(lines[i + 5], "    cmp x1, x0") == 0)) {
		fprintf(out, "%s\n", lines[i]);
		if (!arm64_emit_x1_from_x0_x9_load(out, lines[i + 1]))
			return 0;
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 5]);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(44) &&
	    i + 5 < count &&
	    arm64_is_large_frame_base_x9(lines[i]) &&
	    arm64_is_x0_x9_load(lines[i + 1]) &&
	    arm64_is_push_x0_sp16(lines[i + 2]) &&
	    arm64_is_x0_non_x9_compare_load(lines[i + 3]) &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    (STRCMP(lines[i + 5], "    cmp w1, w0") == 0 ||
	     STRCMP(lines[i + 5], "    cmp x1, x0") == 0)) {
		fprintf(out, "%s\n", lines[i]);
		if (!arm64_emit_x1_from_x0_x9_load(out, lines[i + 1]))
			return 0;
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 5]);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(48) &&
	    i + 8 < count &&
	    arm64_is_large_frame_base_x9(lines[i]) &&
	    arm64_is_x0_x9_load(lines[i + 1]) &&
	    arm64_is_push_x0_sp16(lines[i + 2]) &&
	    arm64_is_large_frame_base_x9(lines[i + 3]) &&
	    arm64_is_x0_x9_load(lines[i + 4]) &&
	    arm64_is_push_x0_sp16(lines[i + 5]) &&
	    STRCMP(lines[i + 6], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 7], "    ldr x1, [sp], #16") == 0 &&
	    (STRCMP(lines[i + 8], "    cmp w1, w0") == 0 ||
	     STRCMP(lines[i + 8], "    cmp x1, x0") == 0)) {
		fprintf(out, "%s\n", lines[i]);
		if (!arm64_emit_xreg_from_x0_x9_load(out, "x1", lines[i + 1]))
			return 0;
		fprintf(out, "%s\n", lines[i + 3]);
		if (!arm64_emit_xreg_from_x0_x9_load(out, "x0", lines[i + 4]))
			return 0;
		fprintf(out, "%s\n", lines[i + 8]);
		*index = i + 9;
		return 1;
	}
	if (arm64_peephole_rule_enabled(49) &&
	    i + 10 < count &&
	    arm64_is_x0_value_equivalent_line(lines[i]) &&
	    strncmp(lines[i + 1], "    ldr w0, [x0", 15) == 0 &&
	    arm64_is_push_x0_sp16(lines[i + 2]) &&
	    arm64_is_x0_value_equivalent_line(lines[i + 3]) &&
	    strncmp(lines[i + 4], "    ldr w0, [x0", 15) == 0 &&
	    STRCMP(lines[i + 5], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 6], "    sub x0, x1, x0") == 0 &&
	    arm64_is_push_x0_sp16(lines[i + 7]) &&
	    arm64_is_x0_local_int_load(lines[i + 8]) &&
	    STRCMP(lines[i + 9], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 10], "    cmp w1, w0") == 0) {
		if (!arm64_emit_xreg_value_equivalent(out, "x1", lines[i]))
			return 0;
		if (!arm64_emit_w1_deref_from_x1_equivalent(out, lines[i + 1]))
			return 0;
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "    sub w1, w1, w0\n");
		fprintf(out, "%s\n", lines[i + 8]);
		fprintf(out, "%s\n", lines[i + 10]);
		*index = i + 11;
		return 1;
	}
	if (arm64_peephole_rule_enabled(50) &&
	    i + 4 < count &&
	    arm64_is_x0_int_value_equivalent_line(lines[i]) &&
	    arm64_is_push_x0_sp16(lines[i + 1]) &&
	    arm64_is_x0_local_int_load(lines[i + 2]) &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    cmp w1, w0") == 0) {
		if (!arm64_emit_w1_int_value_equivalent(out, lines[i]))
			return 0;
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 4]);
		*index = i + 5;
		return 1;
	}
	if (arm64_peephole_rule_enabled(51) &&
	    i + 4 < count &&
	    strncmp(lines[i], "    adrp x0, ", 13) == 0 &&
	    (strncmp(lines[i + 1], "    add x0, x0, ", 16) == 0 ||
	     strncmp(lines[i + 1], "    add  x0, x0, ", 17) == 0) &&
	    arm64_is_push_x0_sp16(lines[i + 2]) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 3], "x0", 0) &&
	    (STRCMP(lines[i + 4], "    add sp, sp, #16") == 0 ||
	     STRCMP(lines[i + 4], "    add sp, sp, #0x10") == 0)) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 5;
		return 1;
	}
	if (arm64_peephole_rule_enabled(51) &&
	    i + 3 < count &&
	    arm64_is_x0_value_equivalent_line(lines[i]) &&
	    arm64_is_push_x0_sp16(lines[i + 1]) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 2], "x0", 0) &&
	    (STRCMP(lines[i + 3], "    add sp, sp, #16") == 0 ||
	     STRCMP(lines[i + 3], "    add sp, sp, #0x10") == 0)) {
		fprintf(out, "%s\n", lines[i]);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(52) &&
	    i + 1 < count &&
	    arm64_is_redundant_x9_stack_self_store(lines[i], lines[i + 1])) {
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(53) &&
	    i + 1 < count &&
	    arm64_parse_mov_x9_imm(lines[i], &imm) &&
	    STRCMP(lines[i + 1], "    cmp w0, w9") == 0 &&
	    imm <= 4095u) {
		fprintf(out, "    cmp w0, #%lu\n", imm);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(54) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldrsw x9, [", 15) == 0 &&
	    STRCMP(lines[i + 1], "    sxtw x9, w9") == 0) {
		fprintf(out, "%s\n", lines[i]);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(54) &&
	    i + 2 < count &&
	    strncmp(lines[i], "    ldrsw x9, [", 15) == 0 &&
	    arm64_is_safe_between_x9_base_reuse(lines[i + 1]) &&
	    STRCMP(lines[i + 2], "    sxtw x9, w9") == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 3;
		return 1;
	}
	if (arm64_peephole_rule_enabled(55) &&
	    i + 4 < count &&
	    strncmp(lines[i], "    adrp x8, ", 13) == 0 &&
	    (strncmp(lines[i + 1], "    add x8, x8, ", 16) == 0 ||
	     strncmp(lines[i + 1], "    add  x8, x8, ", 17) == 0) &&
	    arm64_parse_mov_x9_imm(lines[i + 2], &imm) &&
	    STRCMP(lines[i + 3], "    add x11, x8, x9") == 0 &&
	    STRCMP(lines[i + 4], "    ldrsw x9, [x11]") == 0 &&
	    imm <= 16380u &&
	    (imm & 3u) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    ldrsw x9, [x8, #%lu]\n", imm);
		*index = i + 5;
		return 1;
	}
	if (arm64_peephole_rule_enabled(82) &&
	    i + 4 < count &&
	    strncmp(lines[i], "    adrp x1, ", 13) == 0 &&
	    (strncmp(lines[i + 1], "    add x1, x1, ", 16) == 0 ||
	     strncmp(lines[i + 1], "    add  x1, x1, ", 17) == 0) &&
	    arm64_parse_mov_x9_imm(lines[i + 2], &imm) &&
	    STRCMP(lines[i + 3], "    add x1, x1, x9") == 0 &&
	    STRCMP(lines[i + 4], "    ldrsw x0, [x1]") == 0 &&
	    imm <= 16380u &&
	    (imm & 3u) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    ldrsw x0, [x1, #%lu]\n", imm);
		*index = i + 5;
		return 1;
	}
	if (arm64_peephole_rule_enabled(83) &&
	    i + 5 < count &&
	    strncmp(lines[i], "    adrp x0, ", 13) == 0 &&
	    strncmp(lines[i + 1], "    ldr  x0, [x0, ", 18) == 0 &&
	    arm64_parse_mov_x9_imm(lines[i + 2], &imm) &&
	    STRCMP(lines[i + 3], "    add x0, x0, x9") == 0 &&
	    STRCMP(lines[i + 4], "    mov x17, x0") == 0 &&
	    STRCMP(lines[i + 5], "    str wzr, [x17, #0]") == 0 &&
	    imm <= 16380u &&
	    (imm & 3u) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    str wzr, [x0, #%lu]\n", imm);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(83) &&
	    i + 6 < count &&
	    strncmp(lines[i], "    adrp x0, ", 13) == 0 &&
	    strncmp(lines[i + 1], "    ldr  x0, [x0, ", 18) == 0 &&
	    arm64_parse_mov_x9_imm(lines[i + 2], &imm) &&
	    STRCMP(lines[i + 3], "    add x0, x0, x9") == 0 &&
	    STRCMP(lines[i + 4], "    mov x17, x0") == 0 &&
	    arm64_parse_mov_x9_imm(lines[i + 5], &imm2) &&
	    STRCMP(lines[i + 6], "    str w9, [x17, #0]") == 0 &&
	    imm <= 16380u &&
	    (imm & 3u) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 5]);
		fprintf(out, "    str w9, [x0, #%lu]\n", imm);
		*index = i + 7;
		return 1;
	}
	if (arm64_peephole_rule_enabled(56) &&
	    i + 4 < count &&
	    arm64_parse_mov_reg_imm(lines[i], "x2", &imm) &&
	    (STRCMP(lines[i + 1], "    movz x1, #0") == 0 ||
	     STRCMP(lines[i + 1], "    mov x1, #0") == 0) &&
	    strncmp(lines[i + 2], "    adrp x0, ", 13) == 0 &&
	    (strncmp(lines[i + 3], "    add x0, x0, ", 16) == 0 ||
	     strncmp(lines[i + 3], "    add  x0, x0, ", 17) == 0 ||
	     strncmp(lines[i + 3], "    ldr  x0, [x0, ", 18) == 0) &&
	    STRCMP(lines[i + 4], "    bl _memset") == 0 &&
	    imm <= 65535u) {
		fprintf(out, "    movz x1, #%lu\n", imm);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "    bl _bzero\n");
		*index = i + 5;
		return 1;
	}
	if (arm64_peephole_rule_enabled(58) &&
	    i + 3 < count &&
	    STRCMP(lines[i], "    mov x9, x0") == 0 &&
	    strncmp(lines[i + 1], "    add x9, x9, #", 17) == 0 &&
	    strncmp(lines[i + 2], "    and x9, x9, #", 17) == 0 &&
	    STRCMP(lines[i + 3], "    mov x0, x9") == 0) {
		fprintf(out, "    add x0, x0, #%s\n", lines[i + 1] + 17);
		fprintf(out, "    and x0, x0, #%s\n", lines[i + 2] + 17);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(58) &&
	    i + 1 < count &&
	    STRCMP(lines[i], "    mov x9, x0") == 0 &&
	    STRCMP(lines[i + 1], "    mov x0, x9") == 0) {
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(60) &&
	    i + 1 < count &&
	    arm64_parse_add_x0_x0_imm(lines[i], &imm) &&
	    STRCMP(lines[i + 1], "    mov x17, x0") == 0) {
		fprintf(out, "    add x17, x0, #%lu\n", imm);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(60) &&
	    i + 1 < count &&
	    STRCMP(lines[i], "    sxtw x0, w0") == 0 &&
	    STRCMP(lines[i + 1], "    ret") == 0) {
		*index = i + 1;
		return 1;
	}
	if (arm64_peephole_rule_enabled(60) &&
	    i + 2 < count &&
	    STRCMP(lines[i], "    sxtw x0, w0") == 0 &&
	    strncmp(lines[i + 1], "    add sp, sp, #", 17) == 0 &&
	    STRCMP(lines[i + 2], "    ret") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 3;
		return 1;
	}
	if (arm64_peephole_rule_enabled(60) &&
	    i + 3 < count &&
	    STRCMP(lines[i], "    sxtw x0, w0") == 0 &&
	    STRCMP(lines[i + 1], "    mov sp, x29") == 0 &&
	    STRCMP(lines[i + 2], "    ldp x29, x30, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    ret") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(61) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldursw x0, ", 15) == 0 &&
	    STRCMP(lines[i + 1], "    mov x1, x0") == 0) {
		fprintf(out, "    ldursw x1, %s\n", lines[i] + 15);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(61) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldrsw x0, ", 14) == 0 &&
	    STRCMP(lines[i + 1], "    mov x1, x0") == 0) {
		fprintf(out, "    ldrsw x1, %s\n", lines[i] + 14);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(65) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldursw x0, ", 15) == 0 &&
	    arm64_parse_mov_arg_x0(lines[i + 1], reg, sizeof(reg))) {
		fprintf(out, "    ldursw %s, %s\n", reg, lines[i] + 15);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(65) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldrsw x0, ", 14) == 0 &&
	    arm64_parse_mov_arg_x0(lines[i + 1], reg, sizeof(reg))) {
		fprintf(out, "    ldrsw %s, %s\n", reg, lines[i] + 14);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(65) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldursb x0, ", 15) == 0 &&
	    arm64_parse_mov_arg_x0(lines[i + 1], reg, sizeof(reg))) {
		fprintf(out, "    ldursb %s, %s\n", reg, lines[i] + 15);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(65) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    ldrsb x0, ", 14) == 0 &&
	    arm64_parse_mov_arg_x0(lines[i + 1], reg, sizeof(reg))) {
		fprintf(out, "    ldrsb %s, %s\n", reg, lines[i] + 14);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(68) &&
	    i + 3 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 1], "    ldursw x0, ", 15) == 0 &&
	    STRCMP(lines[i + 2], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    cmp w1, w0") == 0) {
		fprintf(out, "    ldursw x1, %s\n", lines[i + 1] + 15);
		fprintf(out, "    cmp w0, w1\n");
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(68) &&
	    i + 3 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 1], "    ldrsw x0, ", 14) == 0 &&
	    STRCMP(lines[i + 2], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    cmp w1, w0") == 0) {
		fprintf(out, "    ldrsw x1, %s\n", lines[i + 1] + 14);
		fprintf(out, "    cmp w0, w1\n");
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(69) &&
	    i + 4 < count &&
	    (strncmp(lines[i], "    ldursw x0, ", 15) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, ", 14) == 0) &&
	    strncmp(lines[i + 1], "    adrp x1, ", 13) == 0 &&
	    strncmp(lines[i + 2], "    str w0, [x1, ",
		    strlen("    str w0, [x1, ")) == 0 &&
	    STRCMP(lines[i + 3], lines[i]) == 0 &&
	    strncmp(lines[i + 4], "    cbnz x0, ", 13) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 4]);
		*index = i + 5;
		return 1;
	}
	if (arm64_peephole_rule_enabled(69) &&
	    i + 5 < count &&
	    (strncmp(lines[i], "    ldursw x0, ", 15) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, ", 14) == 0) &&
	    strncmp(lines[i + 1], "    adrp x1, ", 13) == 0 &&
	    strncmp(lines[i + 2], "    str w0, [x1, ",
		    strlen("    str w0, [x1, ")) == 0 &&
	    strncmp(lines[i + 3], "    .loc ", 9) == 0 &&
	    STRCMP(lines[i + 4], lines[i]) == 0 &&
	    strncmp(lines[i + 5], "    cbnz x0, ", 13) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 5]);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(70) &&
	    i + 3 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 1], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 2], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    sub x0, x1, w0, sxtw #4") == 0) {
		fprintf(out, "    sub x0, x0, #16\n");
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(71) &&
	    i + 5 < count &&
	    strncmp(lines[i], "    ldr x0, [x29, ", 18) == 0 &&
	    STRCMP(lines[i + 1], "    ldr x0, [x0, #0]") == 0 &&
	    strncmp(lines[i + 2], "    str x0, [x29, ", 18) == 0 &&
	    strncmp(lines[i + 3], "    .loc ", 9) == 0 &&
	    strncmp(lines[i + 4], "    ldr x0, [x29, ", 18) == 0 &&
	    strncmp(lines[i + 5], "    ldr x1, [x29, ", 18) == 0 &&
	    STRCMP(lines[i + 5] + 18, lines[i + 2] + 18) == 0) {
		fprintf(out, "    ldr x1, [x29, %s\n", lines[i] + 18);
		fprintf(out, "    ldr x1, [x1, #0]\n");
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 4]);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(72) &&
	    i + 6 < count &&
	    arm64_parse_mov_reg_imm(lines[i], "x2", &imm2) &&
	    imm2 == 8 &&
	    arm64_parse_frame_negative_offset(lines[i + 1], "ldr x0", &off2) &&
	    arm64_parse_add_x0_x0_imm(lines[i + 2], &imm) &&
	    (imm & 7u) == 0 &&
	    imm <= 32760u &&
	    STRCMP(lines[i + 3], "    mov x1, x0") == 0 &&
	    arm64_parse_sub_reg_reg_imm(lines[i + 4], "x0", "x29", &off0) &&
	    STRCMP(lines[i + 5], "    bl _memcpy") == 0 &&
	    arm64_parse_frame_negative_offset(lines[i + 6], "ldr x0", &off1) &&
	    off0 == off1) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    ldr x0, [x0, #%lu]\n", imm);
		*index = i + 7;
		return 1;
	}
	if (arm64_peephole_rule_enabled(72) &&
	    i + 7 < count &&
	    arm64_parse_mov_reg_imm(lines[i], "x2", &imm2) &&
	    imm2 == 8 &&
	    arm64_parse_frame_negative_offset(lines[i + 1], "ldr x0", &off2) &&
	    arm64_parse_add_x0_x0_imm(lines[i + 2], &imm) &&
	    (imm & 7u) == 0 &&
	    imm <= 32760u &&
	    STRCMP(lines[i + 3], "    mov x1, x0") == 0 &&
	    arm64_parse_sub_reg_reg_imm(lines[i + 4], "x0", "x29", &off0) &&
	    STRCMP(lines[i + 5], "    bl _memcpy") == 0 &&
	    strncmp(lines[i + 6], "    .loc ", 9) == 0 &&
	    arm64_parse_frame_negative_offset(lines[i + 7], "ldr x0", &off1) &&
	    off0 == off1) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    ldr x0, [x0, #%lu]\n", imm);
		fprintf(out, "%s\n", lines[i + 6]);
		*index = i + 8;
		return 1;
	}
	if (arm64_peephole_rule_enabled(73) &&
	    i + 9 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    imm <= 4095u &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_is_x0_value_equivalent_line(lines[i + 2]) &&
	    strncmp(lines[i + 3], "    ldrsw x0, [x0",
		    strlen("    ldrsw x0, [x0")) == 0 &&
	    STRCMP(lines[i + 4], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 5], &imm2) &&
	    imm2 <= 65535u &&
	    STRCMP(lines[i + 6], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 7], "    mul x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 8], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 9], "    add x0, x1, x0") == 0) {
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "    movz x1, #%lu\n", imm2);
		fprintf(out, "    mul x0, x0, x1\n");
		if (imm != 0)
			fprintf(out, "    add x0, x0, #%lu\n", imm);
		*index = i + 10;
		return 1;
	}
	if (arm64_peephole_rule_enabled(74) &&
	    i + 9 < count &&
	    (strncmp(lines[i], "    ldursw x0, ", 15) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, ", 14) == 0) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_is_x0_value_equivalent_line(lines[i + 2]) &&
	    strncmp(lines[i + 3], "    ldrsw x0, [x0",
		    strlen("    ldrsw x0, [x0")) == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    add x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 6], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 7], lines[i + 2]) == 0 &&
	    STRCMP(lines[i + 8], "    ldr x1, [sp], #16") == 0 &&
	    strncmp(lines[i + 9], "    bl ", 7) == 0) {
		if (!arm64_emit_xreg_signed_load_equivalent(out, "x1", lines[i]))
			return 0;
		fprintf(out, "%s\n", lines[i + 2]);
		if (!arm64_emit_xreg_signed_load_equivalent(out, "x9", lines[i + 3]))
			return 0;
		fprintf(out, "    add x1, x1, x9\n");
		fprintf(out, "%s\n", lines[i + 9]);
		*index = i + 10;
		return 1;
	}
	if (arm64_peephole_rule_enabled(75) &&
	    i + 8 < count &&
	    (strncmp(lines[i], "    ldursw x0, ", 15) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, ", 14) == 0) &&
	    arm64_parse_adrp_page_x1(lines[i + 1], symbol, sizeof(symbol)) &&
	    arm64_parse_add_pageoff_x1(lines[i + 2], symbol) &&
	    strncmp(lines[i + 3], "    str w0, [x1", 15) == 0 &&
	    strncmp(lines[i + 4], "    .loc ", 9) == 0 &&
	    STRCMP(lines[i + 5], lines[i]) == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 6], symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    arm64_parse_add_pageoff_x1(lines[i + 7], symbol) &&
	    strncmp(lines[i + 8], "    str w0, [x1", 15) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "%s\n", lines[i + 8]);
		*index = i + 9;
		return 1;
	}
	if (arm64_peephole_rule_enabled(76) &&
	    i + 9 < count &&
	    arm64_is_x0_value_equivalent_line(lines[i]) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 2], "    adrp x1, ", 13) == 0 &&
	    strncmp(lines[i + 3], "    ldr x0, [x1, ", 17) == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    sub x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 6], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 7], &imm) &&
	    STRCMP(lines[i + 8], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 9], "    sdiv x0, x1, x0") == 0) {
		if (!arm64_emit_xreg_value_equivalent(out, "x2", lines[i]))
			return 0;
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "    sub x1, x2, x0\n");
		fprintf(out, "    movz x0, #%lu\n", imm);
		fprintf(out, "%s\n", lines[i + 9]);
		*index = i + 10;
		return 1;
	}
	if (arm64_peephole_rule_enabled(77) &&
	    i + 3 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    imm <= 4095u &&
	    arm64_parse_adrp_page_x1(lines[i + 1], symbol, sizeof(symbol)) &&
	    arm64_parse_add_pageoff_x1(lines[i + 2], symbol) &&
	    STRCMP(lines[i + 3], "    add x0, x1, w0, sxtw") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    add x0, x1, #%lu\n", imm);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(77) &&
	    i + 3 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    imm <= 4095u &&
	    arm64_parse_adrp_ldr_got_x1(lines[i + 1], lines[i + 2],
					symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 3], "    add x0, x1, w0, sxtw") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    add x0, x1, #%lu\n", imm);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(66) &&
	    i + 1 < count &&
	    STRCMP(lines[i], "    mov x15, x0") == 0 &&
	    STRCMP(lines[i + 1], "    sxtw x0, w15") == 0) {
		fprintf(out, "    sxtw x0, w0\n");
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(66) &&
	    i + 2 < count &&
	    STRCMP(lines[i], "    mov x15, x0") == 0 &&
	    strncmp(lines[i + 1], "    .loc ", 9) == 0 &&
	    STRCMP(lines[i + 2], "    sxtw x0, w15") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    sxtw x0, w0\n");
		*index = i + 3;
		return 1;
	}
	if (arm64_peephole_rule_enabled(67) &&
	    i + 1 < count &&
	    STRCMP(lines[i], "    mov x10, x0") == 0 &&
	    STRCMP(lines[i + 1], "    add x9, x10, x9") == 0) {
		fprintf(out, "    add x9, x0, x9\n");
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(63) &&
	    i + 1 < count &&
	    strncmp(lines[i], "    add x17, x0, #", 18) == 0) {
		const char *num = lines[i] + 18;

		imm = 0;
		while (*num >= '0' && *num <= '9') {
			imm = imm * 10u + (unsigned long)(*num - '0');
			num++;
		}
		if (*num == '\0') {
			store_count = 0;
			while (i + 1 + store_count < count && store_count < 8 &&
			       arm64_parse_zero_store_x17(lines[i + 1 + store_count],
							  &store_is64[store_count],
							  &store_offset[store_count]) &&
			       arm64_store_unsigned_offset_encodable(
				       store_is64[store_count],
				       imm + store_offset[store_count])) {
				store_count++;
			}
			if (store_count > 0 &&
			    arm64_x17_dead_before_next_use(lines, count, i + 1 + store_count)) {
				for (int si = 0; si < store_count; si++)
					arm64_emit_zero_store_x0(out, store_is64[si],
								imm + store_offset[si]);
				*index = i + 1 + store_count;
				return 1;
			}
		}
	}
	if (arm64_peephole_rule_enabled(64) &&
	    i + 3 < count &&
	    arm64_parse_add_x0_x0_imm(lines[i], &imm) &&
	    STRCMP(lines[i + 1], "    mov x2, x0") == 0 &&
	    arm64_parse_sub_reg_reg_imm(lines[i + 2], "x1", "x2", &off0) &&
	    arm64_parse_sub_reg_reg_imm(lines[i + 3], "x0", "x1", &off1) &&
	    imm >= off0 + off1 &&
	    imm <= 4095u &&
	    imm - off0 <= 4095u &&
	    imm - off0 - off1 <= 4095u) {
		fprintf(out, "    add x2, x0, #%lu\n", imm);
		fprintf(out, "    add x1, x0, #%lu\n", imm - off0);
		fprintf(out, "    add x0, x0, #%lu\n", imm - off0 - off1);
		*index = i + 4;
		return 1;
	}
	if (arm64_peephole_rule_enabled(41) &&
	    i + 5 < count &&
	    arm64_is_x0_value_equivalent_line(lines[i]) &&
	    arm64_is_push_x0_sp16(lines[i + 1]) &&
	    arm64_parse_adrp_ldr_got_x0(lines[i + 2], lines[i + 3],
					symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    cmp x1, x0") == 0) {
		if (!arm64_emit_xreg_value_equivalent(out, "x1", lines[i]))
			return 0;
		fprintf(out, "    adrp x0, %s@GOTPAGE\n", symbol);
		fprintf(out, "    ldr  x0, [x0, %s@GOTPAGEOFF]\n", symbol);
		fprintf(out, "    cmp x1, x0\n");
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(41) &&
	    i + 5 < count &&
	    arm64_is_x0_value_equivalent_line(lines[i]) &&
	    arm64_is_push_x0_sp16(lines[i + 1]) &&
	    arm64_parse_adrp_add_page_x0(lines[i + 2], lines[i + 3],
					 symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    cmp x1, x0") == 0) {
		if (!arm64_emit_xreg_value_equivalent(out, "x1", lines[i]))
			return 0;
		fprintf(out, "    adrp x0, %s@PAGE\n", symbol);
		fprintf(out, "    add  x0, x0, %s@PAGEOFF\n", symbol);
		fprintf(out, "    cmp x1, x0\n");
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(42) &&
	    i + 16 < count &&
	    arm64_is_large_frame_base_x9(lines[i]) &&
	    (strncmp(lines[i + 1], "    ldrsw x0, [x9", 17) == 0 ||
	     strncmp(lines[i + 1], "    ldr x0, [x9", 15) == 0 ||
	     strncmp(lines[i + 1], "    ldr w0, [x9", 15) == 0 ||
	     strncmp(lines[i + 1], "    ldrb w0, [x9", 16) == 0 ||
	     strncmp(lines[i + 1], "    ldrh w0, [x9", 16) == 0) &&
	    strncmp(lines[i + 2], "    sub x9, x29, #", 18) == 0 &&
	    arm64_is_push_x0_sp16(lines[i + 6]) &&
	    arm64_is_x0_value_equivalent_line(lines[i + 7]) &&
	    arm64_is_push_x0_sp16(lines[i + 8]) &&
	    arm64_is_large_frame_base_x9(lines[i + 9]) &&
	    arm64_is_plain_x9_mem_access(lines[i + 10]) &&
	    arm64_is_push_x0_sp16(lines[i + 11]) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 12], "x0", 0) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 13], "x1", 16) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 14], "x2", 32) &&
	    arm64_is_add_sp_sp_48(lines[i + 15]) &&
	    strncmp(lines[i + 16], "    bl", 6) == 0 &&
	    STRCMP(lines[i + 3], "    sxtw x0, w0") == 0 &&
	    strncmp(lines[i + 4], "    movz x10, #", 15) == 0 &&
	    STRCMP(lines[i + 5], "    madd x0, x0, x10, x9") == 0) {
		fprintf(out, "%s\n", lines[i]);
		if (!arm64_emit_xreg_from_x0_x9_load(out, "x2", lines[i + 1]))
			return 0;
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    sxtw x2, w2\n");
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "    madd x2, x2, x10, x9\n");
		if (!arm64_emit_xreg_value_equivalent(out, "x1", lines[i + 7]))
			return 0;
		fprintf(out, "%s\n", lines[i + 9]);
		fprintf(out, "%s\n", lines[i + 10]);
		fprintf(out, "%s\n", lines[i + 16]);
		*index = i + 17;
		return 1;
	}
	if (arm64_peephole_rule_enabled(43) &&
	    i + 5 < count &&
	    arm64_is_push_x0_sp16(lines[i]) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 1], "x0", 0) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 2], "x1", 16) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 3], "x2", 32) &&
	    arm64_is_add_sp_sp_48(lines[i + 4]) &&
	    strncmp(lines[i + 5], "    bl", 6) == 0) {
		fprintf(out, "    ldr x1, [sp, #0]\n");
		fprintf(out, "    ldr x2, [sp, #16]\n");
		fprintf(out, "    add sp, sp, #32\n");
		fprintf(out, "%s\n", lines[i + 5]);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(45) &&
	    i + 6 < count &&
	    arm64_is_push_x0_sp16(lines[i]) &&
	    arm64_is_large_frame_base_x9(lines[i + 1]) &&
	    strncmp(lines[i + 2], "    ldrsw x0, [x9", 17) == 0 &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    sxtw x0, w0") == 0 &&
	    strncmp(lines[i + 5], "    movz x2, #", 14) == 0 &&
	    STRCMP(lines[i + 6], "    madd x0, x0, x2, x1") == 0) {
		fprintf(out, "    mov x1, x0\n");
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "%s\n", lines[i + 5]);
		fprintf(out, "%s\n", lines[i + 6]);
		*index = i + 7;
		return 1;
	}
	if (arm64_peephole_rule_enabled(47) &&
	    i + 5 < count &&
	    arm64_is_push_x0_sp16(lines[i]) &&
	    arm64_is_x0_local_int_load(lines[i + 1]) &&
	    STRCMP(lines[i + 2], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    sxtw x0, w0") == 0 &&
	    strncmp(lines[i + 4], "    movz x2, #", 14) == 0 &&
	    STRCMP(lines[i + 5], "    madd x0, x0, x2, x1") == 0) {
		fprintf(out, "    mov x1, x0\n");
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "%s\n", lines[i + 5]);
		*index = i + 6;
		return 1;
	}
	if (arm64_peephole_rule_enabled(46) &&
	    i + 11 < count &&
	    arm64_is_push_x0_sp16(lines[i]) &&
	    STRCMP(lines[i + 1], "    mov x17, x0") == 0 &&
	    STRCMP(lines[i + 2], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    ldrsw x0, [x17]") == 0 &&
	    arm64_is_push_x0_sp16(lines[i + 4]) &&
	    STRCMP(lines[i + 5], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 6], "    mov x0, x1") == 0 &&
	    STRCMP(lines[i + 7], "    add x0, x0, #1") == 0 &&
	    arm64_is_push_x0_sp16(lines[i + 8]) &&
	    STRCMP(lines[i + 9], "    str w0, [x17]") == 0 &&
	    STRCMP(lines[i + 10], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 11], "    ldr x0, [sp], #16") == 0) {
		fprintf(out, "    mov x17, x0\n");
		fprintf(out, "    ldrsw x0, [x17]\n");
		fprintf(out, "    add w9, w0, #1\n");
		fprintf(out, "    str w9, [x17]\n");
		*index = i + 12;
		return 1;
	}
	if (arm64_peephole_rule_enabled(37) &&
	    i + 1 < count &&
	    arm64_is_zero_mov_x0(lines[i]) &&
	    arm64_emit_zero_frame_store(out, lines[i + 1])) {
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(36) &&
	    arm64_parse_unconditional_branch_label(lines[i], symbol, sizeof(symbol))) {
		int j = i + 1;

		while (j < count) {
			if (arm64_is_loc_line(lines[j])) {
				j++;
				continue;
			}
			if (arm64_parse_local_label_name(lines[j], symbol2, sizeof(symbol2))) {
				if (STRCMP(symbol, symbol2) == 0) {
					int k;

					for (k = i + 1; k <= j; k++)
						fprintf(out, "%s\n", lines[k]);
					*index = j + 1;
					return 1;
				}
				j++;
				continue;
			}
			break;
		}
	}

	if (arm64_peephole_rule_enabled(59) &&
	    i + 1 < count &&
	    STRCMP(lines[i], "    mov sp, x29") == 0 &&
	    STRCMP(lines[i + 1], "    ldp x29, x30, [sp], #16") == 0 &&
	    arm64_can_omit_frame_restore(lines, i)) {
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(62) &&
	    i + 1 < count &&
	    (STRCMP(lines[i], "    add sp, sp, #16") == 0 ||
	     STRCMP(lines[i], "    add sp, sp, #0x10") == 0) &&
	    STRCMP(lines[i + 1], "    mov sp, x29") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(62) &&
	    i + 2 < count &&
	    (STRCMP(lines[i], "    add sp, sp, #16") == 0 ||
	     STRCMP(lines[i], "    add sp, sp, #0x10") == 0) &&
	    arm64_parse_local_label_name(lines[i + 1], symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 2], "    mov sp, x29") == 0) {
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "%s\n", lines[i + 2]);
		*index = i + 3;
		return 1;
	}
	if (arm64_peephole_rule_enabled(1) &&
	    i + 1 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 1], "    ldr x0, [sp], #16") == 0) {
		*index = i + 2;
		return 1;
	}
	if (arm64_peephole_rule_enabled(2) &&
	    i + 16 < count &&
	    STRCMP(lines[i], "    stp x29, x30, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 1], "    mov x29, sp") == 0 &&
	    lines[i + 2][0] == 'L') {
		int j = i + 3;
		if (j < count && arm64_is_loc_line(lines[j]))
			j++;
		if (j + 12 < count &&
		    strncmp(lines[j], "    adrp x1, ", 13) == 0 &&
		    strstr(lines[j + 1], "    ldrsw x0, [x1, ") == lines[j + 1] &&
		    STRCMP(lines[j + 2], "    sxtw x0, w0") == 0 &&
		    STRCMP(lines[j + 3], "    cmp w0, #0") == 0 &&
		    strncmp(lines[j + 4], "    b.le ", 10) == 0 &&
		    strncmp(lines[j + 5], "    adrp x0, ", 13) == 0 &&
		    strncmp(lines[j + 6], "    add", 7) == 0 &&
		    strstr(lines[j + 6], "x0, x0, ") != NULL &&
		    strncmp(lines[j + 7], "    b ", 6) == 0 &&
		    lines[j + 8][0] == 'L' &&
		    strncmp(lines[j + 9], "    adrp x0, ", 13) == 0 &&
		    strncmp(lines[j + 10], "    add", 7) == 0 &&
		    strstr(lines[j + 10], "x0, x0, ") != NULL &&
		    lines[j + 11][0] == 'L' &&
		    STRCMP(lines[j + 12], "    ldp x29, x30, [sp], #16") == 0 &&
		    j + 13 < count &&
		    STRCMP(lines[j + 13], "    ret") == 0) {
			const char *true_add = strstr(lines[j + 6], "x0, x0, ");
			const char *false_add = strstr(lines[j + 10], "x0, x0, ");

			fprintf(out, "    adrp x8, %s\n", lines[j] + 13);
			fprintf(out, "    ldr w10, [x8, %s\n", lines[j + 1] + 18);
			fprintf(out, "    adrp x9, %s\n", lines[j + 9] + 13);
			if (strstr(lines[j + 10], "    add  ") == lines[j + 10])
				fprintf(out, "    add  x9, x9, %s\n", false_add + 8);
			else
				fprintf(out, "    add x9, x9, %s\n", false_add + 8);
			fprintf(out, "    adrp x8, %s\n", lines[j + 5] + 13);
			if (strstr(lines[j + 6], "    add  ") == lines[j + 6])
				fprintf(out, "    add  x8, x8, %s\n", true_add + 8);
			else
				fprintf(out, "    add x8, x8, %s\n", true_add + 8);
			fprintf(out, "    subs w10, w10, #0\n");
			fprintf(out, "    csel x0, x8, x9, gt\n");
			fprintf(out, "    ret\n");
			*index = j + 14;
			return 1;
		}
	}

	if (arm64_peephole_rule_enabled(3) &&
	    i + 5 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 1], &imm) &&
	    imm == 0 &&
	    STRCMP(lines[i + 2], "    str xzr, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 3], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    arm64_emit_direct_zero_store(out, lines[i + 5])) {
		*index = i + 6;
		return 1;
	}

	if (arm64_peephole_rule_enabled(4) &&
	    i + 5 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_is_zero_mov_x0(lines[i + 1]) &&
	    STRCMP(lines[i + 2], "    str xzr, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 3], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    (STRCMP(lines[i + 5], "    cmp x1, x0") == 0 ||
	     STRCMP(lines[i + 5], "    cmp w1, w0") == 0)) {
		if (lines[i + 5][8] == 'x')
			fprintf(out, "    cmp x0, #0\n");
		else
			fprintf(out, "    cmp w0, #0\n");
		*index = i + 6;
		return 1;
	}

	if (arm64_peephole_rule_enabled(5) &&
	    i + 4 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 1], "    str xzr, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 2], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    (STRCMP(lines[i + 4], "    cmp x1, x0") == 0 ||
	     STRCMP(lines[i + 4], "    cmp w1, w0") == 0)) {
		if (lines[i + 4][8] == 'x')
			fprintf(out, "    cmp x0, #0\n");
		else
			fprintf(out, "    cmp w0, #0\n");
		*index = i + 5;
		return 1;
	}

	if (arm64_peephole_rule_enabled(6) &&
	    i + 9 < count &&
	    strstr(lines[i], "    ldrsw x0, [x29, ") == lines[i] &&
	    STRCMP(lines[i + 1], "    cmp w0, #4") == 0 &&
	    strncmp(lines[i + 2], "    b.ne ", 9) == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 3], &imm) &&
	    STRCMP(lines[i + 4], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 5], "    b ", 6) == 0 &&
	    lines[i + 6][0] == 'L' &&
	    arm64_parse_mov_x0_imm(lines[i + 7], &imm) &&
	    STRCMP(lines[i + 8], "    str x0, [sp, #-16]!") == 0 &&
	    lines[i + 9][0] == 'L') {
		unsigned long true_imm;
		unsigned long false_imm;

		if (!arm64_parse_mov_x0_imm(lines[i + 3], &true_imm))
			return 0;
		if (!arm64_parse_mov_x0_imm(lines[i + 7], &false_imm))
			return 0;
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    mov w9, #%lu\n", false_imm);
		fprintf(out, "    mov w8, #%lu\n", true_imm);
		fprintf(out, "    subs w10, w0, #4\n");
		fprintf(out, "    csel w0, w8, w9, eq\n");
		fprintf(out, "    str x0, [sp, #-16]!\n");
		*index = i + 10;
		return 1;
	}

	if (arm64_peephole_rule_enabled(7) &&
	    i + 2 < count &&
	    STRCMP(lines[i], "    sxtb x0, w0") == 0 &&
	    STRCMP(lines[i + 1], "    cmp w0, #0") == 0 &&
	    (STRCMP(lines[i + 2], "    cset w0, eq") == 0 ||
	     STRCMP(lines[i + 2], "    cset w0, ne") == 0)) {
		fprintf(out, "    cmp w0, #0\n");
		fprintf(out, "%s\n", lines[i + 2]);
		*index = i + 3;
		return 1;
	}

	if (arm64_peephole_rule_enabled(7) &&
	    i + 2 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    STRCMP(lines[i + 1], "    cmp w0, #0") == 0 &&
	    arm64_is_lowbit_mask_imm(imm) &&
	    (STRCMP(lines[i + 2], "    cset w0, eq") == 0 ||
	     STRCMP(lines[i + 2], "    cset w0, ne") == 0)) {
		if (imm == 0) {
			if (STRCMP(lines[i + 2], "    cset w0, eq") == 0)
				fprintf(out, "    movz x0, #1\n");
			else
				fprintf(out, "    movz x0, #0\n");
		} else {
			fprintf(out, "    tst x0, #%lu\n", imm);
			fprintf(out, "%s\n", lines[i + 2]);
		}
		*index = i + 3;
		return 1;
	}

	if (arm64_peephole_rule_enabled(8) &&
	    i + 2 < count &&
	    arm64_parse_and_x0_x0_imm(lines[i], &imm) &&
	    STRCMP(lines[i + 1], "    cmp w0, #0") == 0 &&
	    (STRCMP(lines[i + 2], "    cset w0, eq") == 0 ||
	     STRCMP(lines[i + 2], "    cset w0, ne") == 0)) {
		fprintf(out, "    tst x0, #%lu\n", imm);
		fprintf(out, "%s\n", lines[i + 2]);
		*index = i + 3;
		return 1;
	}

	if (arm64_peephole_rule_enabled(9) &&
	    i + 14 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 1], "    adrp x0, ", 13) == 0 &&
	    strncmp(lines[i + 2], "    add", 7) == 0 &&
	    strstr(lines[i + 2], "x0, x0, ") != NULL &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 4], "    adrp x1, ", 13) == 0 &&
	    strstr(lines[i + 5], "    ldr x1, [x1, ") == lines[i + 5] &&
	    STRCMP(lines[i + 6], "    ldr x0, [x1]") == 0 &&
	    STRCMP(lines[i + 7], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 8], "    ldr x0, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 9], "    ldr x1, [sp, #16]") == 0 &&
	    STRCMP(lines[i + 10], "    add sp, sp, #32") == 0 &&
	    STRCMP(lines[i + 11], "    ldr x9, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 12], "    str x9, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 13], "    bl _fprintf") == 0 &&
	    STRCMP(lines[i + 14], "    add sp, sp, #16") == 0) {
		const char *add_suffix = strstr(lines[i + 2], "x0, x0, ");

		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    adrp x1, %s\n", lines[i + 1] + 13);
		if (strstr(lines[i + 2], "    add  ") == lines[i + 2])
			fprintf(out, "    add  x1, x1, %s\n", add_suffix + 8);
		else
			fprintf(out, "    add x1, x1, %s\n", add_suffix + 8);
		fprintf(out, "    adrp x9, %s\n", lines[i + 4] + 13);
		fprintf(out, "    ldr x9, [x9, %s\n", lines[i + 5] + 17);
		fprintf(out, "    ldr x0, [x9]\n");
		fprintf(out, "%s\n", lines[i + 13]);
		fprintf(out, "%s\n", lines[i + 14]);
		*index = i + 15;
		return 1;
	}

	if (arm64_peephole_rule_enabled(10) &&
	    i + 13 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 1], "    adrp x0, ", 13) == 0 &&
	    strncmp(lines[i + 2], "    add", 7) == 0 &&
	    strstr(lines[i + 2], "x0, x0, ") != NULL &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 4], "    adrp x1, ", 13) == 0 &&
	    strstr(lines[i + 5], "    ldr x1, [x1, ") == lines[i + 5] &&
	    STRCMP(lines[i + 6], "    ldr x0, [x1]") == 0 &&
	    STRCMP(lines[i + 7], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 8], "    ldr x0, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 9], "    ldr x1, [sp, #16]") == 0 &&
	    STRCMP(lines[i + 10], "    ldr d16, [sp, #32]") == 0 &&
	    STRCMP(lines[i + 11], "    str d16, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 12], "    bl _fprintf") == 0 &&
	    STRCMP(lines[i + 13], "    add sp, sp, #48") == 0) {
		const char *add_suffix = strstr(lines[i + 2], "x0, x0, ");

		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    adrp x1, %s\n", lines[i + 1] + 13);
		if (strstr(lines[i + 2], "    add  ") == lines[i + 2])
			fprintf(out, "    add  x1, x1, %s\n", add_suffix + 8);
		else
			fprintf(out, "    add x1, x1, %s\n", add_suffix + 8);
		fprintf(out, "    adrp x9, %s\n", lines[i + 4] + 13);
		fprintf(out, "    ldr x9, [x9, %s\n", lines[i + 5] + 17);
		fprintf(out, "    ldr x0, [x9]\n");
		fprintf(out, "%s\n", lines[i + 12]);
		fprintf(out, "    add sp, sp, #16\n");
		*index = i + 14;
		return 1;
	}

	if (arm64_peephole_rule_enabled(11) &&
	    i + 5 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 1], &imm) &&
	    STRCMP(lines[i + 2], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 3], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0) {
		if (STRCMP(lines[i + 5], "    add x0, x1, x0") == 0 && imm <= 4095) {
			if (imm != 0)
				fprintf(out, "    add x0, x0, #%lu\n", imm);
			*index = i + 6;
			return 1;
		}

		if (STRCMP(lines[i + 5], "    sub x0, x1, x0") == 0 && imm <= 4095) {
			if (imm != 0)
				fprintf(out, "    sub x0, x0, #%lu\n", imm);
			*index = i + 6;
			return 1;
		}

		if (STRCMP(lines[i + 5], "    mul x0, x1, x0") == 0 &&
		    arm64_parse_power2_shift(imm, &imm2)) {
			if (imm2 != 0)
				fprintf(out, "    lsl x0, x0, #%lu\n", imm2);
			*index = i + 6;
			return 1;
		}

		if (STRCMP(lines[i + 5], "    cmp w1, w0") == 0 && imm <= 4095) {
			fprintf(out, "    cmp w0, #%lu\n", imm);
			*index = i + 6;
			return 1;
		}

		if (STRCMP(lines[i + 5], "    and x0, x1, x0") == 0 &&
		    arm64_is_lowbit_mask_imm(imm)) {
			if (i + 7 < count &&
			    STRCMP(lines[i + 6], "    cmp w0, #0") == 0 &&
			    (STRCMP(lines[i + 7], "    cset w0, eq") == 0 ||
			     STRCMP(lines[i + 7], "    cset w0, ne") == 0)) {
				if (imm == 0) {
					if (STRCMP(lines[i + 7], "    cset w0, eq") == 0)
						fprintf(out, "    movz x0, #1\n");
					else
						fprintf(out, "    movz x0, #0\n");
				} else {
					fprintf(out, "    tst x0, #%lu\n", imm);
					fprintf(out, "%s\n", lines[i + 7]);
				}
				*index = i + 8;
				return 1;
			}
			if (imm == 0)
				fprintf(out, "    movz x0, #0\n");
			else
				fprintf(out, "    and x0, x0, #%lu\n", imm);
			*index = i + 6;
			return 1;
		}
	}

	if (arm64_peephole_rule_enabled(13) &&
	    i + 17 < count &&
	    strncmp(lines[i], "    adrp x1, ", 13) == 0 &&
	    strncmp(lines[i + 1], "    ldrsw x0, [x1, ", 19) == 0 &&
	    STRCMP(lines[i + 2], "    mov w0, w0") == 0 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 4], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 5], "    str x0, [sp, #-16]!") == 0 &&
	    strncmp(lines[i + 6], "    ldrsw x0, [x29, ", 20) == 0 &&
	    STRCMP(lines[i + 7], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 8], "    lsl x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 9], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 10], "    and x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 11], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_is_zero_mov_x0(lines[i + 12]) &&
	    STRCMP(lines[i + 13], "    str xzr, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 14], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 15], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 16], "    cmp w1, w0") == 0 &&
	    strncmp(lines[i + 17], "    b.", 6) == 0) {
		fprintf(out, "%s\n", lines[i + 6]);
		fprintf(out, "    mov w1, #1\n");
		fprintf(out, "    lsl x0, x1, x0\n");
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    ldr w1, [x1, %s\n", lines[i + 1] + 19);
		fprintf(out, "    tst x1, x0\n");
		fprintf(out, "%s\n", lines[i + 17]);
		*index = i + 18;
		return 1;
	}

	if (arm64_peephole_rule_enabled(14) &&
	    i + 8 < count &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    and x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 5], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 6], lines[i + 2]) == 0 &&
	    STRCMP(lines[i + 7], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 8], "    cmp x1, x0") == 0 &&
	    arm64_emit_x1_load_equivalent(out, lines[i + 2])) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    and x0, x0, x1\n");
		fprintf(out, "    cmp x0, x1\n");
		*index = i + 9;
		return 1;
	}

	if (arm64_peephole_rule_enabled(15) &&
	    i + 14 < count &&
	    (strncmp(lines[i], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i], "    ldr x0, [sp", 15) == 0) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 2], "    ldursw x0, [x29", 19) == 0 ||
	     strncmp(lines[i + 2], "    ldrsw x0, [x29", 18) == 0 ||
	     strncmp(lines[i + 2], "    ldr w0, [x29", 16) == 0) &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 4], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 5], "    mov x0, x1") == 0 &&
	    STRCMP(lines[i + 6], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_is_zero_mov_x0(lines[i + 7]) == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 7], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 8], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 9], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 10], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 11], "    add x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 13], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 14], "    ldr x1, [sp], #16") == 0 &&
	    i + 16 < count &&
	    STRCMP(lines[i + 15], "    add x0, x1, w0, sxtw #3") == 0 &&
	    STRCMP(lines[i + 16], "    str x0, [sp, #-16]!") == 0) {
		if (!arm64_emit_x1_load_equivalent(out, lines[i]))
			return 0;
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    add x1, x1, w0, sxtw #3\n");
		fprintf(out, "    add x0, x0, #0x1\n");
		if (!arm64_emit_w0_store_equivalent(out, lines[i + 2]))
			return 0;
		fprintf(out, "    str x1, [sp, #-16]!\n");
		*index = i + 17;
		return 1;
	}

	if (arm64_peephole_rule_enabled(16) &&
	    i + 7 < count &&
	    (strncmp(lines[i], "    ldursw x0, [x29", 19) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, [x29", 18) == 0 ||
	     strncmp(lines[i], "    ldr w0, [x29", 16) == 0 ||
	     STRCMP(lines[i], "    ldrsw x0, [x17]") == 0 ||
	     STRCMP(lines[i], "    ldr w0, [x17]") == 0) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 2], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 6], "    add x0, x1, x0") == 0 &&
	    (strncmp(lines[i + 7], "    stur w0, [x29", 17) == 0 ||
	     strncmp(lines[i + 7], "    str w0, [x29", 16) == 0 ||
	     STRCMP(lines[i + 7], "    str w0, [x17]") == 0)) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    add x0, x0, #0x1\n");
		fprintf(out, "%s\n", lines[i + 7]);
		*index = i + 8;
		return 1;
	}

	if (arm64_peephole_rule_enabled(17) &&
	    i + 2 < count &&
	    STRCMP(lines[i], "    mov x1, x0") == 0 &&
	    arm64_parse_small_signed_x0_imm(lines[i + 1], &simm) &&
	    STRCMP(lines[i + 2], "    add x0, x1, x0") == 0) {
		if (simm == 1) {
			fprintf(out, "    add x0, x0, #0x1\n");
			*index = i + 3;
			return 1;
		}
		if (simm == -1) {
			fprintf(out, "    sub x0, x0, #0x1\n");
			*index = i + 3;
			return 1;
		}
	}

	if (arm64_peephole_rule_enabled(18) &&
	    i + 11 < count &&
	    (strncmp(lines[i], "    ldursw x0, [x29", 19) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, [x29", 18) == 0) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 2], &imm) &&
	    imm <= 65535ul &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    mul x0, x1, x0") == 0 &&
	    arm64_parse_add_x0_x0_imm(lines[i + 5], &imm2) &&
	    STRCMP(lines[i + 6], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_adrp_ldr_got_x0(lines[i + 7], lines[i + 8],
					symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 9], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 10], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 11], "    add x0, x1, w0, sxtw") == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    movz x1, #%lu\n", imm);
		fprintf(out, "    mul x0, x0, x1\n");
		fprintf(out, "    adrp x1, %s@GOTPAGE\n", symbol);
		fprintf(out, "    ldr  x1, [x1, %s@GOTPAGEOFF]\n", symbol);
		fprintf(out, "    add x0, x1, x0\n");
		if (imm2 != 0)
			fprintf(out, "    add x0, x0, #%lu\n", imm2);
		*index = i + 12;
		return 1;
	}

	if (arm64_peephole_rule_enabled(19) &&
	    i + 5 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_adrp_add_page_x0(lines[i + 1], lines[i + 2],
					 symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 3], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    add x0, x1, w0, sxtw") == 0) {
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    add x1, x1, %s@PAGEOFF\n", symbol);
		fprintf(out, "    add x0, x1, w0, sxtw\n");
		*index = i + 6;
		return 1;
	}

	if (arm64_peephole_rule_enabled(20) &&
	    i + 5 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_adrp_ldr_got_x0(lines[i + 1], lines[i + 2],
					symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 3], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    add x0, x1, w0, sxtw") == 0) {
		fprintf(out, "    adrp x1, %s@GOTPAGE\n", symbol);
		fprintf(out, "    ldr  x1, [x1, %s@GOTPAGEOFF]\n", symbol);
		fprintf(out, "    add x0, x1, w0, sxtw\n");
		*index = i + 6;
		return 1;
	}

	if (arm64_peephole_rule_enabled(21) &&
	    i + 5 < count &&
	    (strncmp(lines[i], "    ldursw x0, [x29", 19) == 0 ||
	     strncmp(lines[i], "    ldrsw x0, [x29", 18) == 0 ||
	     strncmp(lines[i], "    ldr w0, [x29", 16) == 0) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 2], &imm) &&
	    imm == 2 &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    mul x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 5], "    str x0, [sp, #-16]!") == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    lsl x0, x0, #1\n");
		fprintf(out, "%s\n", lines[i + 5]);
		*index = i + 6;
		return 1;
	}

	if (arm64_peephole_rule_enabled(22) &&
	    i + 19 < count &&
	    (strncmp(lines[i], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i], "    ldr x0, [sp", 15) == 0) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 2], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i + 2], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i + 2], "    ldr x0, [sp", 15) == 0) &&
	    arm64_parse_add_x0_x0_imm(lines[i + 3], &imm) &&
	    STRCMP(lines[i + 4], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 5], "    mov x17, x0") == 0 &&
	    STRCMP(lines[i + 6], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 7], "    ldrsw x0, [x17]") == 0 &&
	    STRCMP(lines[i + 8], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 9], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 10], "    mov x0, x1") == 0 &&
	    STRCMP(lines[i + 11], "    add x0, x0, #1") == 0 &&
	    STRCMP(lines[i + 12], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 13], "    str w0, [x17]") == 0 &&
	    STRCMP(lines[i + 14], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 15], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 16], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 17], "    sxtw x0, w0") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 18], &imm2) &&
	    STRCMP(lines[i + 19], "    madd x0, x0, x2, x1") == 0) {
		if (!arm64_emit_x1_load_equivalent(out, lines[i]))
			return 0;
		if (!arm64_emit_x17_load_equivalent(out, lines[i + 2]))
			return 0;
		if (imm != 0)
			fprintf(out, "    add x17, x17, #%lu\n", imm);
		fprintf(out, "    ldrsw x0, [x17]\n");
		fprintf(out, "    add w9, w0, #1\n");
		fprintf(out, "    str w9, [x17]\n");
		fprintf(out, "    sxtw x0, w0\n");
		fprintf(out, "    movz x2, #%lu\n", imm2);
		fprintf(out, "    madd x0, x0, x2, x1\n");
		*index = i + 20;
		return 1;
	}

	if (arm64_peephole_rule_enabled(23) &&
	    i + 6 < count &&
	    (strncmp(lines[i], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i], "    ldr x0, [sp", 15) == 0) &&
	    strncmp(lines[i + 1], "    ldrsw x0, [x0", 17) == 0 &&
	    STRCMP(lines[i + 2], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 3], &imm) &&
	    imm == 2 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    mul x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 6], "    str x0, [sp, #-16]!") == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "    ldrsw x0, %s\n", lines[i + 1] + 14);
		fprintf(out, "    lsl x0, x0, #1\n");
		fprintf(out, "%s\n", lines[i + 6]);
		*index = i + 7;
		return 1;
	}

	if (arm64_peephole_rule_enabled(24) &&
	    i + 20 < count &&
	    (strncmp(lines[i], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i], "    ldr x0, [sp", 15) == 0) &&
	    strncmp(lines[i + 1], "    ldr x0, [x0", 15) == 0 &&
	    STRCMP(lines[i + 2], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 3], "    ldur x0, [x29", 17) == 0 ||
	     strncmp(lines[i + 3], "    ldr x0, [x29", 16) == 0 ||
	     strncmp(lines[i + 3], "    ldr x0, [sp", 15) == 0) &&
	    arm64_parse_add_x0_x0_imm(lines[i + 4], &imm) &&
	    STRCMP(lines[i + 5], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 6], "    mov x17, x0") == 0 &&
	    STRCMP(lines[i + 7], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 8], "    ldrsw x0, [x17]") == 0 &&
	    STRCMP(lines[i + 9], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 10], "    mov x1, x0") == 0 &&
	    STRCMP(lines[i + 11], "    mov x0, x1") == 0 &&
	    STRCMP(lines[i + 12], "    add x0, x0, #1") == 0 &&
	    STRCMP(lines[i + 13], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 14], "    str w0, [x17]") == 0 &&
	    STRCMP(lines[i + 15], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 16], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 17], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 18], "    sxtw x0, w0") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 19], &imm2) &&
	    STRCMP(lines[i + 20], "    madd x0, x0, x2, x1") == 0) {
		if (!arm64_emit_x1_load_equivalent(out, lines[i]))
			return 0;
		if (!arm64_emit_x1_deref_from_x1_equivalent(out, lines[i + 1]))
			return 0;
		if (!arm64_emit_x17_load_equivalent(out, lines[i + 3]))
			return 0;
		if (imm != 0)
			fprintf(out, "    add x17, x17, #%lu\n", imm);
		fprintf(out, "    ldrsw x0, [x17]\n");
		fprintf(out, "    add w9, w0, #1\n");
		fprintf(out, "    str w9, [x17]\n");
		fprintf(out, "    sxtw x0, w0\n");
		fprintf(out, "    movz x2, #%lu\n", imm2);
		fprintf(out, "    madd x0, x0, x2, x1\n");
		*index = i + 21;
		return 1;
	}

	if (arm64_peephole_rule_enabled(26) &&
	    i + 8 < count &&
	    arm64_parse_adrp_ldr_got_x0(lines[i], lines[i + 1],
					symbol, sizeof(symbol)) &&
	    STRCMP(lines[i + 2], "    mov x17, x0") == 0 &&
	    arm64_parse_zero_store_x17(lines[i + 3], &is64_0, &off0) &&
	    arm64_is_loc_line(lines[i + 4]) &&
	    arm64_parse_adrp_ldr_got_x0(lines[i + 5], lines[i + 6],
					symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    STRCMP(lines[i + 7], "    mov x17, x0") == 0 &&
	    arm64_parse_zero_store_x17(lines[i + 8], &is64_1, &off1) &&
	    i + 9 < count &&
	    arm64_parse_zero_store_x17(lines[i + 9], &is64_2, &off2)) {
		fprintf(out, "    adrp x17, %s@GOTPAGE\n", symbol);
		fprintf(out, "    ldr  x17, [x17, %s@GOTPAGEOFF]\n", symbol);
		arm64_emit_zero_store_x17(out, is64_0, off0);
		fprintf(out, "%s\n", lines[i + 4]);
		arm64_emit_zero_store_x17(out, is64_1, off1);
		arm64_emit_zero_store_x17(out, is64_2, off2);
		*index = i + 10;
		return 1;
	}

	if (arm64_peephole_rule_enabled(26) &&
	    i + 9 < count &&
	    arm64_parse_adrp_ldr_got_x0(lines[i], lines[i + 1],
					symbol, sizeof(symbol)) &&
	    arm64_parse_add_x0_x0_imm(lines[i + 2], &imm) &&
	    STRCMP(lines[i + 3], "    mov x17, x0") == 0 &&
	    arm64_parse_zero_store_x17(lines[i + 4], &is64_0, &off0) &&
	    arm64_is_loc_line(lines[i + 5]) &&
	    arm64_parse_adrp_ldr_got_x0(lines[i + 6], lines[i + 7],
					symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    STRCMP(lines[i + 8], "    mov x17, x0") == 0 &&
	    arm64_parse_zero_store_x17(lines[i + 9], &is64_1, &off1) &&
	    i + 10 < count &&
	    arm64_parse_zero_store_x17(lines[i + 10], &is64_2, &off2)) {
		fprintf(out, "    adrp x17, %s@GOTPAGE\n", symbol);
		fprintf(out, "    ldr  x17, [x17, %s@GOTPAGEOFF]\n", symbol);
		arm64_emit_zero_store_x17(out, is64_0, imm + off0);
		fprintf(out, "%s\n", lines[i + 5]);
		arm64_emit_zero_store_x17(out, is64_1, off1);
		arm64_emit_zero_store_x17(out, is64_2, off2);
		*index = i + 11;
		return 1;
	}

	if (arm64_peephole_rule_enabled(27) &&
	    i + 4 < count &&
	    ((strncmp(lines[i], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i], "    ldr x0, [sp", 15) == 0)) &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 2], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i + 2], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i + 2], "    ldr x0, [sp", 15) == 0)) &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    str x0, [x1]") == 0) {
		if (!arm64_emit_x1_load_equivalent(out, lines[i]))
			return 0;
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    str x0, [x1]\n");
		*index = i + 5;
		return 1;
	}

	if (arm64_peephole_rule_enabled(28) &&
	    i + 6 < count &&
	    STRCMP(lines[i], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 1], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i + 1], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i + 1], "    ldr x0, [sp", 15) == 0)) &&
	    STRCMP(lines[i + 2], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 3], "    ldr x0, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp, #16]") == 0 &&
	    STRCMP(lines[i + 5], "    add sp, sp, #32") == 0 &&
	    strncmp(lines[i + 6], "    bl ", 7) == 0) {
		fprintf(out, "%s\n", lines[i]);
		fprintf(out, "%s\n", lines[i + 1]);
		fprintf(out, "    ldr x1, [sp], #16\n");
		fprintf(out, "%s\n", lines[i + 6]);
		*index = i + 7;
		return 1;
	}

	if (arm64_peephole_rule_enabled(29) &&
	    i + 18 < count &&
	    STRCMP(lines[i], "    str xzr, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 1], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i + 1], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i + 1], "    ldr x0, [sp", 15) == 0)) &&
	    STRCMP(lines[i + 2], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 3], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i + 3], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i + 3], "    ldr x0, [sp", 15) == 0)) &&
	    STRCMP(lines[i + 4], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 5], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i + 5], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i + 5], "    ldr x0, [sp", 15) == 0)) &&
	    strncmp(lines[i + 6], "    bl ", 7) == 0 &&
	    STRCMP(lines[i + 7], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 8], "    ldur x0, [x29", 17) == 0) ||
	     (strncmp(lines[i + 8], "    ldr x0, [x29", 16) == 0) ||
	     (strncmp(lines[i + 8], "    ldr x0, [sp", 15) == 0)) &&
	    STRCMP(lines[i + 9], "    str x0, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 10], "    str xzr, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 11], "    ldr x0, [sp, #0]") == 0 &&
	    STRCMP(lines[i + 12], "    ldr x1, [sp, #16]") == 0 &&
	    STRCMP(lines[i + 13], "    ldr x2, [sp, #32]") == 0 &&
	    STRCMP(lines[i + 14], "    ldr x3, [sp, #48]") == 0 &&
	    STRCMP(lines[i + 15], "    ldr x4, [sp, #64]") == 0 &&
	    STRCMP(lines[i + 16], "    ldr x5, [sp, #80]") == 0 &&
	    STRCMP(lines[i + 17], "    add sp, sp, #96") == 0 &&
	    strncmp(lines[i + 18], "    bl ", 7) == 0) {
		fprintf(out, "%s\n", lines[i + 5]);
		fprintf(out, "%s\n", lines[i + 6]);
		fprintf(out, "    mov x2, x0\n");
		if (!arm64_emit_x1_load_equivalent(out, lines[i + 8]))
			return 0;
		if (!arm64_emit_xreg_load_equivalent(out, "x3", lines[i + 3]))
			return 0;
		if (!arm64_emit_xreg_load_equivalent(out, "x4", lines[i + 1]))
			return 0;
		fprintf(out, "    movz x0, #0\n");
		fprintf(out, "    movz x5, #0\n");
		fprintf(out, "%s\n", lines[i + 18]);
		*index = i + 19;
		return 1;
	}

	if (arm64_peephole_rule_enabled(30) &&
	    i + 11 < count &&
	    arm64_parse_adrp_page_x1(lines[i], symbol, sizeof(symbol)) &&
	    arm64_parse_ldrsw_pageoff_x1(lines[i + 1], symbol) &&
	    STRCMP(lines[i + 2], "    mov w0, w0") == 0 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 4], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 5], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 6], "    ldrsw x0, [x29", 18) == 0) ||
	     (strncmp(lines[i + 6], "    ldursw x0, [x29", 19) == 0) ||
	     (strncmp(lines[i + 6], "    ldr w0, [x29", 16) == 0)) &&
	    STRCMP(lines[i + 7], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 8], "    lsl x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 9], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 10], "    orr x0, x1, x0") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 11], symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    i + 12 < count &&
	    arm64_parse_str_w0_pageoff_x1(lines[i + 12], symbol)) {
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    ldr w8, [x1, %s@PAGEOFF]\n", symbol);
		fprintf(out, "    movz x9, #1\n");
		fprintf(out, "%s\n", lines[i + 6]);
		fprintf(out, "    lsl x0, x9, x0\n");
		fprintf(out, "    orr w0, w8, w0\n");
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    str w0, [x1, %s@PAGEOFF]\n", symbol);
		*index = i + 13;
		return 1;
	}

	if (arm64_peephole_rule_enabled(31) &&
	    i + 15 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    ((strncmp(lines[i + 2], "    ldrsw x0, [x29", 18) == 0) ||
	     (strncmp(lines[i + 2], "    ldursw x0, [x29", 19) == 0) ||
	     (strncmp(lines[i + 2], "    ldr w0, [x29", 16) == 0)) &&
	    STRCMP(lines[i + 3], "    sub x0, x0, #9") == 0 &&
	    STRCMP(lines[i + 4], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 5], "    lsl x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 6], "    str w0, [x29, #-24]") == 0) {
		int j = i + 7;

		if (j < count && arm64_is_loc_line(lines[j]))
			j++;
		if (j + 9 < count &&
		    arm64_parse_adrp_page_x1(lines[j], symbol, sizeof(symbol)) &&
		    arm64_parse_ldrsw_pageoff_x1(lines[j + 1], symbol) &&
		    STRCMP(lines[j + 2], "    mov w0, w0") == 0 &&
		    STRCMP(lines[j + 3], "    str x0, [sp, #-16]!") == 0 &&
		    STRCMP(lines[j + 4], "    ldr w0, [x29, #-24]") == 0 &&
		    STRCMP(lines[j + 5], "    mvn x0, x0") == 0 &&
		    STRCMP(lines[j + 6], "    ldr x1, [sp], #16") == 0 &&
		    STRCMP(lines[j + 7], "    and x0, x1, x0") == 0 &&
		    arm64_parse_adrp_page_x1(lines[j + 8], symbol2, sizeof(symbol2)) &&
		    STRCMP(symbol, symbol2) == 0 &&
		    arm64_parse_str_w0_pageoff_x1(lines[j + 9], symbol)) {
			fprintf(out, "    movz x8, #1\n");
			fprintf(out, "%s\n", lines[i + 2]);
			fprintf(out, "    sub x0, x0, #9\n");
			fprintf(out, "    lsl x8, x8, x0\n");
			fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
			fprintf(out, "    ldr w9, [x1, %s@PAGEOFF]\n", symbol);
			fprintf(out, "    mvn w8, w8\n");
			fprintf(out, "    and w0, w9, w8\n");
			fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
			fprintf(out, "    str w0, [x1, %s@PAGEOFF]\n", symbol);
			*index = j + 10;
			return 1;
		}
	}

	if (arm64_peephole_rule_enabled(84) &&
	    i + 11 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 2], "    ldr w0, [x29", 16) == 0 ||
	     strncmp(lines[i + 2], "    ldur w0, [x29", 17) == 0) &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    lsl x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 5], "    str w0, [x29, #-16]") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 6], symbol, sizeof(symbol)) &&
	    arm64_parse_ldr_w8_pageoff_x1(lines[i + 7], symbol) &&
	    (strncmp(lines[i + 8], "    ldr w0, [x29", 16) == 0 ||
	     strncmp(lines[i + 8], "    ldur w0, [x29", 17) == 0) &&
	    STRCMP(lines[i + 9], "    orr w0, w8, w0") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 10], symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    arm64_parse_str_w0_pageoff_x1(lines[i + 11], symbol)) {
		fprintf(out, "    movz x9, #1\n");
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    lsl x9, x9, x0\n");
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    ldr w8, [x1, %s@PAGEOFF]\n", symbol);
		fprintf(out, "    orr w0, w8, w9\n");
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    str w0, [x1, %s@PAGEOFF]\n", symbol);
		*index = i + 12;
		return 1;
	}

	if (arm64_peephole_rule_enabled(85) &&
	    i + 15 < count &&
	    arm64_parse_mov_x0_imm(lines[i], &imm) &&
	    imm == 1 &&
	    STRCMP(lines[i + 1], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 2], "    ldr w0, [x29", 16) == 0 ||
	     strncmp(lines[i + 2], "    ldur w0, [x29", 17) == 0) &&
	    STRCMP(lines[i + 3], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 4], "    lsl x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 5], "    str w0, [x29, #-16]") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 6], symbol, sizeof(symbol)) &&
	    arm64_parse_ldrsw_pageoff_x1(lines[i + 7], symbol) &&
	    STRCMP(lines[i + 8], "    mov w0, w0") == 0 &&
	    STRCMP(lines[i + 9], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 10], "    ldr w0, [x29", 16) == 0 ||
	     strncmp(lines[i + 10], "    ldur w0, [x29", 17) == 0) &&
	    STRCMP(lines[i + 11], "    mvn x0, x0") == 0 &&
	    STRCMP(lines[i + 12], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 13], "    and x0, x1, x0") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 14], symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    arm64_parse_str_w0_pageoff_x1(lines[i + 15], symbol)) {
		fprintf(out, "    movz x8, #1\n");
		fprintf(out, "%s\n", lines[i + 2]);
		fprintf(out, "    lsl x8, x8, x0\n");
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    ldr w9, [x1, %s@PAGEOFF]\n", symbol);
		fprintf(out, "    mvn w8, w8\n");
		fprintf(out, "    and w0, w9, w8\n");
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    str w0, [x1, %s@PAGEOFF]\n", symbol);
		*index = i + 16;
		return 1;
	}

	if (arm64_peephole_rule_enabled(32) &&
	    i + 12 < count &&
	    arm64_parse_adrp_page_x1(lines[i], symbol, sizeof(symbol)) &&
	    arm64_parse_ldrsw_pageoff_x1(lines[i + 1], symbol) &&
	    STRCMP(lines[i + 2], "    mov w0, w0") == 0 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 4], "    ldr w0, [x29", 16) == 0 ||
	     strncmp(lines[i + 4], "    ldur w0, [x29", 17) == 0) &&
	    STRCMP(lines[i + 5], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 6], "    and x0, x1, x0") == 0 &&
	    STRCMP(lines[i + 7], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_is_zero_mov_x0(lines[i + 8]) &&
	    STRCMP(lines[i + 9], "    str xzr, [sp, #-16]!") == 0 &&
	    STRCMP(lines[i + 10], "    ldr x0, [sp], #16") == 0 &&
	    STRCMP(lines[i + 11], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 12], "    cmp w1, w0") == 0 &&
	    i + 14 < count &&
	    strncmp(lines[i + 13], "    b.eq ", 9) == 0 &&
	    strncmp(lines[i + 14], "    b ", 6) == 0) {
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    ldr w8, [x1, %s@PAGEOFF]\n", symbol);
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "    tst w8, w0\n");
		fprintf(out, "%s\n", lines[i + 13]);
		fprintf(out, "%s\n", lines[i + 14]);
		*index = i + 15;
		return 1;
	}

	if (arm64_peephole_rule_enabled(33) &&
	    i + 8 < count &&
	    arm64_parse_adrp_page_x1(lines[i], symbol, sizeof(symbol)) &&
	    arm64_parse_ldrsw_pageoff_x1(lines[i + 1], symbol) &&
	    STRCMP(lines[i + 2], "    mov w0, w0") == 0 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    (strncmp(lines[i + 4], "    ldr w0, [x29", 16) == 0 ||
	     strncmp(lines[i + 4], "    ldur w0, [x29", 17) == 0) &&
	    STRCMP(lines[i + 5], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 6], "    orr x0, x1, x0") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 7], symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    arm64_parse_str_w0_pageoff_x1(lines[i + 8], symbol)) {
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    ldr w8, [x1, %s@PAGEOFF]\n", symbol);
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "    orr w0, w8, w0\n");
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    str w0, [x1, %s@PAGEOFF]\n", symbol);
		*index = i + 9;
		return 1;
	}

	if (arm64_peephole_rule_enabled(34) &&
	    i + 8 < count &&
	    arm64_parse_adrp_page_x1(lines[i], symbol, sizeof(symbol)) &&
	    arm64_parse_ldrsw_pageoff_x1(lines[i + 1], symbol) &&
	    STRCMP(lines[i + 2], "    mov w0, w0") == 0 &&
	    STRCMP(lines[i + 3], "    str x0, [sp, #-16]!") == 0 &&
	    arm64_parse_mov_x0_imm(lines[i + 4], &imm) &&
	    arm64_is_lowbit_mask_imm(imm) &&
	    STRCMP(lines[i + 5], "    ldr x1, [sp], #16") == 0 &&
	    STRCMP(lines[i + 6], "    orr x0, x1, x0") == 0 &&
	    arm64_parse_adrp_page_x1(lines[i + 7], symbol2, sizeof(symbol2)) &&
	    STRCMP(symbol, symbol2) == 0 &&
	    arm64_parse_str_w0_pageoff_x1(lines[i + 8], symbol)) {
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    ldr w0, [x1, %s@PAGEOFF]\n", symbol);
		if (imm != 0)
			fprintf(out, "    orr w0, w0, #%lu\n", imm);
		fprintf(out, "    adrp x1, %s@PAGE\n", symbol);
		fprintf(out, "    str w0, [x1, %s@PAGEOFF]\n", symbol);
		*index = i + 9;
		return 1;
	}

	if (arm64_peephole_rule_enabled(35) &&
	    i + 10 < count &&
	    arm64_is_push_x0_sp16(lines[i]) &&
	    arm64_is_x0_value_equivalent_line(lines[i + 1]) &&
	    arm64_is_push_x0_sp16(lines[i + 2]) &&
	    strncmp(lines[i + 3], "    sub x9, x29, #", 18) == 0 &&
	    STRCMP(lines[i + 4], "    ldr x0, [x9]") == 0 &&
	    arm64_is_push_x0_sp16(lines[i + 5]) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 6], "x0", 0) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 7], "x1", 16) &&
	    arm64_is_ldr_xreg_sp_offset(lines[i + 8], "x2", 32) &&
	    arm64_is_add_sp_sp_48(lines[i + 9]) &&
	    strncmp(lines[i + 10], "    bl ", 7) == 0) {
		fprintf(out, "    mov x2, x0\n");
		if (!arm64_emit_xreg_value_equivalent(out, "x1", lines[i + 1]))
			return 0;
		fprintf(out, "%s\n", lines[i + 3]);
		fprintf(out, "%s\n", lines[i + 4]);
		fprintf(out, "%s\n", lines[i + 10]);
		*index = i + 11;
		return 1;
	}

	if (!arm64_peephole_rule_enabled(12))
		return 0;
	if (i + 3 >= count)
		return 0;
	if (STRCMP(lines[i], "    str x0, [sp, #-16]!") != 0)
		return 0;
	if (!arm64_parse_mov_x0_imm(lines[i + 1], &imm))
		return 0;
	if (STRCMP(lines[i + 2], "    ldr x1, [sp], #16") != 0)
		return 0;

	if (STRCMP(lines[i + 3], "    add x0, x1, x0") == 0 && imm <= 4095) {
		if (imm != 0)
			fprintf(out, "    add x0, x0, #%lu\n", imm);
		*index = i + 4;
		return 1;
	}

	if (STRCMP(lines[i + 3], "    sub x0, x1, x0") == 0 && imm <= 4095) {
		if (imm != 0)
			fprintf(out, "    sub x0, x0, #%lu\n", imm);
		*index = i + 4;
		return 1;
	}

	if (STRCMP(lines[i + 3], "    mul x0, x1, x0") == 0 &&
	    arm64_parse_power2_shift(imm, &imm2)) {
		if (imm2 != 0)
			fprintf(out, "    lsl x0, x0, #%lu\n", imm2);
		*index = i + 4;
		return 1;
	}

	if (STRCMP(lines[i + 3], "    cmp w1, w0") == 0 && imm <= 4095) {
		fprintf(out, "    cmp w0, #%lu\n", imm);
		*index = i + 4;
		return 1;
	}

	if (STRCMP(lines[i + 3], "    and x0, x1, x0") == 0 &&
	    arm64_is_lowbit_mask_imm(imm)) {
		if (imm == 0)
			fprintf(out, "    movz x0, #0\n");
		else
			fprintf(out, "    and x0, x0, #%lu\n", imm);
		*index = i + 4;
		return 1;
	}

	return 0;
}

static char *
read_text_line(FILE *in)
{
	char *buf;
	int cap = 128;
	int len = 0;
	int ch;

	if (!in)
		return NULL;

	buf = xmalloc((size_t)cap);
	while ((ch = fgetc(in)) != EOF) {
		if (len + 1 >= cap) {
			cap *= 2;
			buf = xrealloc(buf, (size_t)cap);
		}
		if (ch == '\n')
			break;
		if (ch == '\r')
			continue;
		buf[len++] = (char)ch;
	}

	if (ch == EOF && len == 0) {
		xfree(buf);
		return NULL;
	}

	buf[len] = '\0';
	return buf;
}

int
arm64_peephole_optimize_file(const char *asm_path)
{
	const char *disable_env;
	FILE *in;
	FILE *out;
	char *line = NULL;
	char **lines = NULL;
	int line_count = 0;
	int line_cap_count = 0;
	int changed = 0;
	int ok = 0;
	int rerun = 0;
	char *tmp_path = NULL;
	size_t path_len;

	if (!asm_path || !asm_path[0])
		return 0;
	disable_env = getenv("TCC_DISABLE_ARM64_PEEPHOLE");
	if (disable_env && disable_env[0] && disable_env[0] != '0')
		return 1;

	g_arm64_peephole_depth++;

	in = fopen(asm_path, "r");
	if (!in)
		goto cleanup_return;

	while ((line = read_text_line(in)) != NULL) {
		if (line_count >= line_cap_count) {
			int new_cap = line_cap_count ? line_cap_count * 2 : 1024;
			lines = xrealloc(lines, sizeof(char *) * (size_t)new_cap);
			line_cap_count = new_cap;
		}
		lines[line_count++] = line;
	}
	fclose(in);
	in = NULL;

	path_len = strlen(asm_path);
	tmp_path = xmalloc(path_len + 5);
	memcpy(tmp_path, asm_path, path_len);
	memcpy(tmp_path + path_len, ".opt", 5);

	out = fopen(tmp_path, "w");
	if (!out)
		goto cleanup;

	for (int i = 0; i < line_count; ) {
		int next_i = i;
		if (arm64_is_instruction_line(lines[i]) &&
		    arm64_peephole_try_fold(out, lines, line_count, &next_i)) {
			changed = 1;
			i = next_i;
			continue;
		}
		fprintf(out, "%s\n", lines[i]);
		i++;
	}

	if (fclose(out) != 0) {
		out = NULL;
		goto cleanup;
	}
	out = NULL;

	if (!changed) {
		remove(tmp_path);
		ok = 1;
		goto cleanup;
	}

	if (rename(tmp_path, asm_path) != 0) {
		remove(tmp_path);
		goto cleanup;
	}

	ok = 1;
	rerun = 1;

cleanup:
	if (in)
		fclose(in);
	if (out)
		fclose(out);
	for (int i = 0; i < line_count; i++)
		xfree(lines[i]);
	xfree(lines);
	if (tmp_path)
		xfree(tmp_path);
	if (rerun && g_arm64_peephole_depth < arm64_peephole_max_passes()) {
		ok = arm64_peephole_optimize_file(asm_path);
		if (g_arm64_peephole_depth > 0)
			g_arm64_peephole_depth--;
		return ok;
	}
cleanup_return:
	if (g_arm64_peephole_depth > 0)
		g_arm64_peephole_depth--;
	return ok;
}

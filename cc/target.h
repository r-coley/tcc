#ifndef TCC_TARGET_H
#define TCC_TARGET_H

/*
 * Assembly-object format details for the host assembler.
 *
 * The x64 backend emits GNU/intel assembly, but symbol spelling and section
 * directives differ between Mach-O (Darwin) and ELF (Linux/BSD).  Keep those
 * differences in one place instead of hard-coding Mach-O spellings throughout
 * the backend and global-data emitters.
 */
#if defined(__APPLE__)
#define TCC_ASM_SYM_PREFIX "_"
#define TCC_ASM_CSTRING_SECTION ".section __TEXT,__cstring,cstring_literals"
#define TCC_ASM_CONST_SECTION ".section __TEXT,__const"
#define TCC_ASM_DEBUG_ABBREV_SECTION ".section __DWARF,__debug_abbrev,regular,debug"
#define TCC_ASM_DEBUG_STR_SECTION ".section __DWARF,__debug_str,regular,debug"
#define TCC_ASM_DEBUG_INFO_SECTION ".section __DWARF,__debug_info,regular,debug"
#define TCC_ASM_DEBUG_ADDR_SECTION ".section __DWARF,__debug_addr,regular,debug"
#else
#define TCC_ASM_SYM_PREFIX ""
#define TCC_ASM_CSTRING_SECTION ".section .rodata"
#define TCC_ASM_CONST_SECTION ".section .rodata"
#define TCC_ASM_DEBUG_ABBREV_SECTION ".section .debug_abbrev"
#define TCC_ASM_DEBUG_STR_SECTION ".section .debug_str"
#define TCC_ASM_DEBUG_INFO_SECTION ".section .debug_info"
#define TCC_ASM_DEBUG_ADDR_SECTION ".section .debug_addr"
#endif

/*
 * Target type sizes (bytes).
 *
 * All current tcc targets (arm64, x64, x86, mips) share the same model for
 * the scalar types tcc currently handles.  When a multi-model compiler is
 * needed (e.g. separate ILP32 vs LP64 targets), these become runtime values
 * derived from the active target; for now a single set of compile-time
 * constants avoids magic numbers scattered throughout the source.
 *
 *   TCC_SIZEOF_INT    sizeof(int)           — always 4 on all supported targets
 *   TCC_SIZEOF_LONG   sizeof(long)          — 8 on LP64 (arm64/x64)
 *   TCC_SIZEOF_PTR    sizeof(void *)        — 8 on all 64-bit targets
 *   TCC_SIZEOF_SHORT  sizeof(short)         — always 2
 *   TCC_SIZEOF_CHAR   sizeof(char)          — always 1
 */
#define TCC_SIZEOF_CHAR   1
#define TCC_SIZEOF_SHORT  2
#define TCC_SIZEOF_INT    4
#define TCC_SIZEOF_LONG   8
#define TCC_SIZEOF_PTR    8

/*
 * Natural aligned store widths for struct copy and similar operations.
 * These are the power-of-2 chunk sizes used when copying struct fields
 * or zeroing memory word-by-word. On all current tcc targets the maximum
 * is 8 bytes (64-bit store).
 */
#define TCC_STORE_WIDTH_8   8
#define TCC_STORE_WIDTH_4   4
#define TCC_STORE_WIDTH_2   2
#define TCC_STORE_WIDTH_1   1

#endif /* TCC_TARGET_H */

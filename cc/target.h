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
#else
#define TCC_ASM_SYM_PREFIX ""
#define TCC_ASM_CSTRING_SECTION ".section .rodata"
#endif

#endif /* TCC_TARGET_H */

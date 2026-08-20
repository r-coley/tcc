	.build_version macos, 26, 0	sdk_version 26, 4
	.section	__TEXT,__text,regular,pure_instructions
	.globl	_main                           ; -- Begin function main
	.p2align	2
_main:                                  ; @main
	.cfi_startproc
; %bb.0:
	sub	sp, sp, #16
	.cfi_def_cfa_offset 16
	str	wzr, [sp, #12]
	adrp	x8, _s@PAGE
	adrp	x9, _s@PAGE
	add	x9, x9, _s@PAGEOFF
	ldrsb	w8, [x8, _s@PAGEOFF]
	ldr	w9, [x9, #4]
	add	w0, w8, w9
	add	sp, sp, #16
	ret
	.cfi_endproc
                                        ; -- End function
	.section	__DATA,__data
	.globl	_s                              ; @s
	.p2align	2, 0x0
_s:
	.byte	1                               ; 0x1
	.space	3
	.long	41                              ; 0x29

.subsections_via_symbols

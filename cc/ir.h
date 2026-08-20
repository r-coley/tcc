#ifndef IR_H
#define IR_H

#include <stdint.h>
#include <stddef.h>

#include "ast.h"
#include "codegen/codegen.h"

typedef enum {
	IR_CONST,
	IR_LOAD,
	IR_STORE,
	IR_BINOP,
	IR_UNOP,
	IR_RETURN,
	IR_LABEL,
	IR_BRANCH,
	IR_CALL,
	IR_CALL_INDIRECT,
	IR_CALL_ACC_ARG0,
	IR_CALL_PACKED_GPR,
	IR_ASM,
	IR_POP
} IRKind;

#define IR_SIMPLE_CALL_MAX_ARGS 16

typedef enum {
	IR_SIMPLE_CALL_ARG_NONE = 0,
	IR_SIMPLE_CALL_ARG_CONST,
	IR_SIMPLE_CALL_ARG_STRING,
	IR_SIMPLE_CALL_ARG_LOCAL,
	IR_SIMPLE_CALL_ARG_LOCAL_ADD_IMM,
	IR_SIMPLE_CALL_ARG_LOCAL_MUL_IMM,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_MEMBER_MUL_IMM,
	IR_SIMPLE_CALL_ARG_ADDR_LOCAL,
	IR_SIMPLE_CALL_ARG_ADDR_LOCAL_OFFSET,
	IR_SIMPLE_CALL_ARG_ADDR_GLOBAL,
	IR_SIMPLE_CALL_ARG_ADDR_GLOBAL_OFFSET,
	IR_SIMPLE_CALL_ARG_LOAD_GLOBAL,
	IR_SIMPLE_CALL_ARG_LOAD_GLOBAL_OFFSET_ADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_MEMBER,
	IR_SIMPLE_CALL_ARG_GLOBAL_MEMBER,
	IR_SIMPLE_CALL_ARG_GLOBAL_MEMBER_PTR_OFFSET_ADDR,
	IR_SIMPLE_CALL_ARG_FUNCADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_OFFSET_ADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_INDEXED_LOAD,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_INDEXED_ADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_MEMBER_INDEXED_ADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_MEMBER_MEMBER_ADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_MEMBER_MEMBER,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_OFFSET_INDEXED_LOAD,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_OFFSET_INDEXED_ADDR,
	IR_SIMPLE_CALL_ARG_ADDR_LOCAL_INDEXED_ADDR,
	IR_SIMPLE_CALL_ARG_GLOBAL_MEMBER_INDEXED_LOAD,
	IR_SIMPLE_CALL_ARG_GLOBAL_MEMBER_INDEXED_ADDR,
	IR_SIMPLE_CALL_ARG_LOCAL_PTR_MEMBER_MEMBER_INDEXED_LOAD
} IRSimpleCallArgKind;

typedef struct IRSimpleCallArg {
	uint8_t kind;
	uint8_t reserved[8];
	long value;
	int aux;
	int aux2;
	char *name;
} IRSimpleCallArg;

/* Sub-operation codes — one per distinct op string.
 * Stored as uint8_t subop in IRInst to replace STRCMP dispatch. */
typedef enum {
	/* IR_LOAD / IR_STORE sub-ops */
	IR_OP_LOCAL = 0,    /* "local"       */
	IR_OP_GLOBAL,       /* "global"      */
	IR_OP_DEREF,        /* "deref"       */
	IR_OP_INDEXED,      /* "indexed"     */
	IR_OP_GIDX_LOAD,    /* "gidx_load"   */
	IR_OP_GIDX_STORE,   /* "gidx_store"  */
	IR_OP_GLOBAL_MEMBER,/* "global_member" */
	IR_OP_LOCAL_PTR_MEMBER,/* "local_ptr_member" */
	IR_OP_STRING,       /* "string"      */
	IR_OP_VOLATILE,     /* "volatile"    */
	/* IR_BRANCH sub-ops */
	IR_OP_JMP,          /* "jmp"         */
	IR_OP_JZ,           /* "jz"          */
	IR_OP_JNZ,          /* "jnz"         */
	IR_OP_JE,           /* "je"          */
	IR_OP_JNE,          /* "jne"         */
	IR_OP_JLT,          /* "jlt"         */
	IR_OP_JLE,          /* "jle"         */
	IR_OP_JGT,          /* "jgt"         */
	IR_OP_JGE,          /* "jge"         */
	IR_OP_JULT,         /* "jult"        */
	IR_OP_JULE,         /* "jule"        */
	IR_OP_JUGT,         /* "jugt"        */
	IR_OP_JUGE,         /* "juge"        */
	/* IR_LABEL sub-ops */
	IR_OP_L,            /* "L"           */
	IR_OP_FUNC,         /* "func"        */
	/* IR_UNOP sub-ops */
	IR_OP_NEG,          /* "neg"         */
	IR_OP_NOT,          /* "not"         */
	IR_OP_BITNOT,       /* "bitnot"      */
	IR_OP_CAST,         /* "cast"        */
	IR_OP_SHL_IMM,      /* "shl_imm"     */
	IR_OP_SHR_IMM,      /* "shr_imm"     */
	IR_OP_USHR_IMM,     /* "ushr_imm"    */
	IR_OP_UNSUPPORTED,  /* "unsupported" */
	IR_OP_TMP_TO_ACC,   /* "tmp_to_acc"  */
	IR_OP_ACC_TO_TMP,   /* "acc_to_tmp"  */
	IR_OP_ACC_TO_SAVED, /* "acc_to_saved" */
	IR_OP_STACK_SAVE,   /* "stack_save"   */
	IR_OP_STACK_RESTORE,/* "stack_restore" */
	IR_OP_STACK_ALLOC,  /* "stack_alloc"  */
	IR_OP_VA_START,     /* "va_start"     */
	IR_OP_VIA_SAVED,    /* "via_saved"   */
	IR_OP_VIA_SAVED_OFF,/* "via_saved_off" */
	IR_OP_LOAD_VIA_SAVED,/* "load_via_saved" */
	IR_OP_STRING_ACC,   /* "string_acc"  */
	IR_OP_POP,          /* "pop"         */
	IR_OP_INT_TO_FP_BITS, /* "int_to_fp_bits" */
	IR_OP_FP_BITS_TO_INT, /* "fp_bits_to_int" */
	IR_OP_ARM64_RET_AGG_LOCAL, /* "arm64_ret_agg_local" */
	IR_OP_ARM64_RET_AGG_GLOBAL, /* "arm64_ret_agg_global" */
	IR_OP_ARM64_STORE_RET_AGG_LOCAL, /* "arm64_store_ret_agg_local" */
	IR_OP_ARM64_STORE_RET_AGG_GLOBAL, /* "arm64_store_ret_agg_global" */
	IR_OP_ARM64_RET_HFA_LOCAL, /* "arm64_ret_hfa_local" */
	IR_OP_ARM64_RET_HFA_GLOBAL, /* "arm64_ret_hfa_global" */
	IR_OP_ARM64_STORE_RET_HFA_LOCAL, /* "arm64_store_ret_hfa_local" */
	IR_OP_ARM64_STORE_RET_HFA_GLOBAL, /* "arm64_store_ret_hfa_global" */
	IR_OP_ARM64_ARG_HFA_LOCAL, /* "arm64_arg_hfa_local" */
	IR_OP_ARM64_ARG_HFA_GLOBAL, /* "arm64_arg_hfa_global" */
	IR_OP_X64_RET_FP_AGG_LOCAL, /* "x64_ret_fp_agg_local" */
	IR_OP_X64_RET_FP_AGG_GLOBAL, /* "x64_ret_fp_agg_global" */
	IR_OP_X64_STORE_RET_FP_AGG_LOCAL, /* "x64_store_ret_fp_agg_local" */
	IR_OP_X64_STORE_RET_FP_AGG_GLOBAL, /* "x64_store_ret_fp_agg_global" */
	IR_OP_X64_ARG_FP_AGG_LOCAL, /* "x64_arg_fp_agg_local" */
	IR_OP_X64_ARG_FP_AGG_GLOBAL, /* "x64_arg_fp_agg_global" */
	/* IR_BINOP sub-ops */
	IR_OP_ADD,          /* "add"         */
	IR_OP_SUB,          /* "sub"         */
	IR_OP_MUL,          /* "mul"         */
	IR_OP_DIV,          /* "div"         */
	IR_OP_MOD,          /* "mod"         */
	IR_OP_AND,          /* "and"         */
	IR_OP_OR,           /* "or"          */
	IR_OP_XOR,          /* "xor"         */
	IR_OP_SHL,          /* "shl"         */
	IR_OP_SHR,          /* "shr"         */
	IR_OP_UDIV,         /* "udiv"        */
	IR_OP_UMOD,         /* "umod"        */
	IR_OP_USHR,         /* "ushr"        */
	IR_OP_EQ,           /* "eq"          */
	IR_OP_NE,           /* "ne"          */
	IR_OP_LT,           /* "lt"          */
	IR_OP_LE,           /* "le"          */
	IR_OP_GT,           /* "gt"          */
	IR_OP_GE,           /* "ge"          */
	IR_OP_ULT,          /* "ult"         */
	IR_OP_ULE,          /* "ule"         */
	IR_OP_UGT,          /* "ugt"         */
	IR_OP_UGE,          /* "uge"         */
	IR_OP_LAND,         /* "land"        */
	IR_OP_LOR,          /* "lor"         */
	IR_OP_PTR_ADD,      /* "ptr_add"     */
	IR_OP_PTR_SUB,      /* "ptr_sub"     */
	IR_OP_PTR_COPY,     /* "ptr_copy"    */
	IR_OP_ADD_OFFSET,   /* "add_offset"  */
	/* IR_CALL / address sub-ops */
	IR_OP_FUNCADDR,     /* "funcaddr"    */
	IR_OP_ADDR_LOCAL,   /* "addr_local"  */
	IR_OP_ADDR_GLOBAL,  /* "addr_global" */
	IR_OP_ADDR_GIDX,    /* "addr_gidx"   */
	IR_OP_ADDR_INDEXED, /* "addr_indexed" */
	IR_OP_LOAD_DEREF,   /* "load_deref"  */
	IR_OP_LOAD_INDEXED, /* "load_indexed" */
	IR_OP_MEMBER_PTR,   /* "member_ptr"  */
	IR_OP_RETURN,       /* "return"      */
	IR_OP_NONE,         /* ""            */
	IR_OP_UNKNOWN       /* anything else */
} IRSubOp;

typedef struct IRInst {
	IRKind kind;
	int id;
	/*
	 * Generic operands used by the compact IR.  Their meaning is defined by
	 * the emitting wrapper and the instruction kind/sub-op.  For example:
	 * local load/store: value=stack offset, aux=size;
	 * deref store: value=size; call: value=arg count, aux=discard-result;
	 * cast: value=target size, aux=is-unsigned; scaled ptr binop: aux=scale.
	 * Prefer adding a named ir_emit_* wrapper over writing raw value/aux calls.
	 */
	long value;
	int aux;
	int is_static;
	int is_extern;
	int fixed_params;  /* for IR_CALL: fixed named param count (-1 = not variadic) */
	int result_size;   /* for IR_CALL*: scalar return size when non-zero */
	int result_is_fp;  /* for IR_CALL*: return value is in FP return register */
	int fp_size;       /* for IR_BINOP/IR_BRANCH: operands are floating bits */
	int extra;         /* target-specific auxiliary payload */
	unsigned int fp_arg_mask;        /* for IR_CALL*: arg i is floating */
	unsigned int fp_arg_double_mask; /* for IR_CALL*: arg i is 8-byte float */
	uint8_t simple_call_arg_count;   /* for IR_CALL: direct pure GPR args */
	int src_line;       /* source line number for debug info (0 = unknown) */
	int src_filename_id; /* lexer filename id for debug info */
	char op[24];      /* fixed-size op string kept for dumps/debug */
	uint8_t subop;    /* numeric sub-op code for fast dispatch (IRSubOp) */
	char *name;       /* heap-allocated: variable-length function/variable names */
	IRSimpleCallArg simple_call_args[IR_SIMPLE_CALL_MAX_ARGS];
	char **param_names; /* for IR_LABEL/func debug metadata */
	int *param_type_ids;
	int *param_offsets;
	int *param_abi_sizes;
	char **param_struct_names;
	char **param_pointer_struct_names;
	int *param_pointer_depths;
	NodeDebugLocal *debug_locals;
	int debug_local_count;
	struct IRInst *next;
} IRInst;

typedef struct IRString {
	int label;
	char *value;
	unsigned int len;
	int width;
	struct IRString *next;
} IRString;

typedef struct IRBlock {
	int id;
	int label;
	IRInst *first;
	IRInst *last;
	struct IRBlock *fallthrough;
	struct IRBlock *branch;
	struct IRBlock *next;
} IRBlock;

typedef struct IRUnsupportedFunc {
	char *name;
	char *reason;
} IRUnsupportedFunc;

typedef struct IRFunctionSpan {
	char *name;
	IRInst *start;
	IRInst *end;
} IRFunctionSpan;

typedef struct IREmitProfile {
	double collect_strings;
	double setup;
	double body;
	double debug_finish;
	double named_lookup;
	double named_stream;
} IREmitProfile;

typedef struct IRProgram {
	IRInst *head;
	IRInst *tail;
	int next_id;
	IRString *strings;
	IRBlock *blocks;
	int block_count;
	int next_label_limit;
	int cached_program_label_limit;
	int unsupported_count;
	int unsupported_outside_functions;
	IRUnsupportedFunc *unsupported_funcs;
	int unsupported_func_count;
	int unsupported_func_cap;
	char unsupported_reason[256];
	IRFunctionSpan *function_spans;
	int function_span_count;
	int function_span_cap;
} IRProgram;

IRProgram *ir_build(Node *program);
void ir_set_target_codegen(Codegen *cg);
void ir_dump(IRProgram *program);
void ir_build_cfg(IRProgram *program);
void ir_dump_cfg(IRProgram *program);
void ir_cfg_eliminate_unreachable(IRProgram *program);
void ir_optimize(IRProgram *program, int level);
int ir_can_codegen(IRProgram *program);
int ir_has_unsupported(IRProgram *program);
const char *ir_unsupported_reason(IRProgram *program);
int ir_has_unsupported_outside_functions(IRProgram *program);
int ir_unsupported_function_count(IRProgram *program);
const char *ir_unsupported_function_name(IRProgram *program, int index);
const char *ir_unsupported_function_reason(IRProgram *program, int index);
int ir_program_label_limit(IRProgram *program);
int ir_function_is_supported(IRProgram *program, const char *name);
void ir_emit_named_function(IRProgram *program, Codegen *cg, const char *name);
void ir_emit_program(IRProgram *program, Codegen *cg, int emit_debug_info);
void ir_emit_profile_reset(void);
void ir_emit_profile_get(IREmitProfile *out);
void ir_free(IRProgram *program);

#endif

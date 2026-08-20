typedef struct {
	char c;
	long l;
} S;

typedef union {
	char c;
	long l;
} U;

_Alignas(S) char global_struct_typedef_aligned;
_Alignas(U) char global_union_typedef_aligned;

int
main(void)
{
	_Alignas(S) char local_struct_typedef_aligned = 0;
	_Alignas(U) char local_union_typedef_aligned = 0;

	if (_Alignof(global_struct_typedef_aligned) != _Alignof(S))
		return 1;
	if (_Alignof(global_union_typedef_aligned) != _Alignof(U))
		return 2;
	if (_Alignof(local_struct_typedef_aligned) != _Alignof(S))
		return 3;
	if (_Alignof(local_union_typedef_aligned) != _Alignof(U))
		return 4;

	return 42;
}

struct S {
	char c;
	long l;
};

union U {
	char c;
	long l;
};

_Alignas(struct S) char global_struct_aligned;
_Alignas(union U) char global_union_aligned;

int
main(void)
{
	_Alignas(struct S) char local_struct_aligned = 0;
	_Alignas(union U) char local_union_aligned = 0;

	if (_Alignof(global_struct_aligned) != _Alignof(struct S))
		return 1;
	if (_Alignof(global_union_aligned) != _Alignof(union U))
		return 2;
	if (_Alignof(local_struct_aligned) != _Alignof(struct S))
		return 3;
	if (_Alignof(local_union_aligned) != _Alignof(union U))
		return 4;

	return 42;
}

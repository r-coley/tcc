typedef const int CA3[3];

_Alignas(CA3) char global_typedef_qualified_array_aligned;

int
main(void)
{
	_Alignas(CA3) char local_typedef_qualified_array_aligned = 0;

	if (_Alignof(global_typedef_qualified_array_aligned) != _Alignof(int))
		return 1;
	if (_Alignof(local_typedef_qualified_array_aligned) != _Alignof(int))
		return 2;

	return 42;
}

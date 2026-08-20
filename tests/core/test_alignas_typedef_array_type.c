typedef int A3[3];

_Alignas(A3) char global_typedef_array_aligned;

int
main(void)
{
	_Alignas(A3) char local_typedef_array_aligned = 0;

	if (_Alignof(global_typedef_array_aligned) != _Alignof(int))
		return 1;
	if (_Alignof(local_typedef_array_aligned) != _Alignof(int))
		return 2;

	return 42;
}

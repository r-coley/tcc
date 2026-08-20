typedef int * const volatile CVIP;

_Alignas(CVIP) char global_typedef_const_volatile_pointer_aligned;

int
main(void)
{
	_Alignas(CVIP) char local_typedef_const_volatile_pointer_aligned = 0;

	if (_Alignof(global_typedef_const_volatile_pointer_aligned) != _Alignof(int *))
		return 1;
	if (_Alignof(local_typedef_const_volatile_pointer_aligned) != _Alignof(int *))
		return 2;

	return 42;
}

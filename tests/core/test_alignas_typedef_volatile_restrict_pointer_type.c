typedef int * volatile restrict VRIP;

_Alignas(VRIP) char global_typedef_volatile_restrict_pointer_aligned;

int
main(void)
{
	_Alignas(VRIP) char local_typedef_volatile_restrict_pointer_aligned = 0;

	if (_Alignof(global_typedef_volatile_restrict_pointer_aligned) != _Alignof(int *))
		return 1;
	if (_Alignof(local_typedef_volatile_restrict_pointer_aligned) != _Alignof(int *))
		return 2;

	return 42;
}

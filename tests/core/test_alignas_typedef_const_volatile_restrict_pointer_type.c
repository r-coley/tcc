typedef int * const volatile restrict CVRIP;

_Alignas(CVRIP) char global_typedef_const_volatile_restrict_pointer_aligned;

int
main(void)
{
	_Alignas(CVRIP) char local_typedef_const_volatile_restrict_pointer_aligned = 0;

	if (_Alignof(global_typedef_const_volatile_restrict_pointer_aligned) != _Alignof(int *))
		return 1;
	if (_Alignof(local_typedef_const_volatile_restrict_pointer_aligned) != _Alignof(int *))
		return 2;

	return 42;
}

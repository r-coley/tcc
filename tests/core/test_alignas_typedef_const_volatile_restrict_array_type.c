typedef const volatile restrict int CVRA3[3];

_Alignas(CVRA3) char global_typedef_const_volatile_restrict_array_aligned;

int
main(void)
{
	_Alignas(CVRA3) char local_typedef_const_volatile_restrict_array_aligned = 0;

	if (_Alignof(global_typedef_const_volatile_restrict_array_aligned) != _Alignof(int))
		return 1;
	if (_Alignof(local_typedef_const_volatile_restrict_array_aligned) != _Alignof(int))
		return 2;

	return 42;
}

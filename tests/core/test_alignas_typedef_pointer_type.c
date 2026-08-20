typedef int *IP;

_Alignas(IP) char global_typedef_pointer_aligned;

int
main(void)
{
	_Alignas(IP) char local_typedef_pointer_aligned = 0;

	if (_Alignof(global_typedef_pointer_aligned) != _Alignof(int *))
		return 1;
	if (_Alignof(local_typedef_pointer_aligned) != _Alignof(int *))
		return 2;

	return 42;
}

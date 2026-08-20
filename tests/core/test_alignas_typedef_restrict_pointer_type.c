typedef int * restrict RIP;

_Alignas(RIP) char global_typedef_restrict_pointer_aligned;

int
main(void)
{
	_Alignas(RIP) char local_typedef_restrict_pointer_aligned = 0;

	if (_Alignof(global_typedef_restrict_pointer_aligned) != _Alignof(int *))
		return 1;
	if (_Alignof(local_typedef_restrict_pointer_aligned) != _Alignof(int *))
		return 2;

	return 42;
}

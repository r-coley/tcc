_Alignas(const int[3]) char global_qualified_array_aligned;

int
main(void)
{
	_Alignas(volatile int[3]) char local_qualified_array_aligned = 0;

	if (_Alignof(global_qualified_array_aligned) != _Alignof(int))
		return 1;
	if (_Alignof(local_qualified_array_aligned) != _Alignof(int))
		return 2;

	return 42;
}

_Alignas(int *[3]) char global_ptr_array_aligned;
_Alignas(const volatile int *[3]) char global_cv_ptr_array_aligned;

int
main(void)
{
	_Alignas(int *[3]) char local_ptr_array_aligned = 0;
	_Alignas(const volatile int *[3]) char local_cv_ptr_array_aligned = 0;

	if (((unsigned long)&global_ptr_array_aligned % _Alignof(int *[3])) != 0)
		return 1;
	if (((unsigned long)&global_cv_ptr_array_aligned %
	     _Alignof(const volatile int *[3])) != 0)
		return 2;
	if (((unsigned long)&local_ptr_array_aligned % _Alignof(int *[3])) != 0)
		return 3;
	if (((unsigned long)&local_cv_ptr_array_aligned %
	     _Alignof(const volatile int *[3])) != 0)
		return 4;

	return 42;
}

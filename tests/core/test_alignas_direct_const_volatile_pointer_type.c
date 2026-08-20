_Alignas(const volatile int *) char global_cv_ptr_aligned;

int
main(void)
{
	_Alignas(const volatile int *) char local_cv_ptr_aligned = 0;

	if (((unsigned long)&global_cv_ptr_aligned %
	     _Alignof(const volatile int *)) != 0)
		return 1;
	if (((unsigned long)&local_cv_ptr_aligned %
	     _Alignof(const volatile int *)) != 0)
		return 2;

	return 42;
}

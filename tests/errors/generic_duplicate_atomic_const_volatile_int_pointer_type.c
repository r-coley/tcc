int
main(void)
{
	int x = 0;
	_Atomic(const volatile int *) ptr = &x;

	return _Generic(ptr, _Atomic(const volatile int *): 1, const volatile int *: 2, default: 3);
}

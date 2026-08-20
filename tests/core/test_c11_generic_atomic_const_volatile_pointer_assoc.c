int
main(void)
{
	int x = 0;
	_Atomic(const volatile int *) ptr = &x;

	return _Generic(ptr, _Atomic(const volatile int *): 42, default: 1);
}

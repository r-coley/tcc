int
main(void)
{
	int x = 0;
	_Atomic(volatile int *) ptr = &x;

	return _Generic(ptr, _Atomic(volatile int *): 42, default: 1);
}

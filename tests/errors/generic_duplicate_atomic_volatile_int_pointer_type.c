int
main(void)
{
	int x = 0;
	_Atomic(volatile int *) ptr = &x;

	return _Generic(ptr, _Atomic(volatile int *): 1, volatile int *: 2, default: 3);
}

int
main(void)
{
	int x = 0;
	_Atomic(int *) ptr = &x;

	return _Generic(ptr, _Atomic(int *): 42, default: 1);
}

int
main(void)
{
	return _Alignof(volatile int[2][2]) == _Alignof(int) ? 42 : 1;
}

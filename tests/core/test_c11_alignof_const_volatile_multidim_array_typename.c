int
main(void)
{
	return _Alignof(const volatile int[2][2]) == _Alignof(int) ? 42 : 1;
}

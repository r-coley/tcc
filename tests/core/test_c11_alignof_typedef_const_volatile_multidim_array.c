typedef const volatile int cvmatrix2x2_t[2][2];

int
main(void)
{
	return _Alignof(cvmatrix2x2_t) == _Alignof(int) ? 42 : 1;
}

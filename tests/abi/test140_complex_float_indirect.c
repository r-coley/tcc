#if __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

static int
consume_complex_float_indirect(int left, _Complex float value, int right)
{
	float real = ((float *)&value)[0];
	float imag = ((float *)&value)[1];

	if (left != 7)
		return 1;
	if ((int)real != 19)
		return 2;
	if ((int)imag != 3)
		return 3;
	if (right != 11)
		return 4;
	return 42;
}

int
main(void)
{
	int (*fp)(int, _Complex float, int) = consume_complex_float_indirect;
	_Complex float value = (_Complex float)19.0f;

	((float *)&value)[1] = 3.0f;
	return fp(7, value, 11);
}

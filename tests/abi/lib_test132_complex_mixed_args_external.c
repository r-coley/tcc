#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

int
consume_complex_float_mix(int left, _Complex float value, int right)
{
	if (left != 7)
		return 1;
	if ((float)value != 19.0f)
		return 2;
	if (right != 11)
		return 3;
	return 42;
}

int
consume_complex_double_mix(long left, _Complex double value, long right)
{
	if (left != 5)
		return 4;
	if ((double)value != 24.0)
		return 5;
	if (right != 13)
		return 6;
	return 42;
}

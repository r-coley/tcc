typedef signed char schar;

int
main(void)
{
	volatile schar vsc = 0;
	const volatile int cvi = 2;
	volatile float vf = 3.0f;
	const volatile double cvd = 4.0;
	int total = 0;

	total += _Generic(vsc, schar: 10, default: 100);
	total += _Generic(vsc, signed char: 20, default: 100);
	total += _Generic(cvi, int: 30, default: 100);
	total += _Generic(vf, float: 40, default: 100);
	total += _Generic(cvd, double: 50, default: 100);

	return total == 150 ? 42 : total;
}

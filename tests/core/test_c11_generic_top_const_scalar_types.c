typedef signed char schar;

int
main(void)
{
	schar const sc = 0;
	const int ci = 2;
	const float cf = 3.0f;
	const double cd = 4.0;
	int total = 0;

	total += _Generic(sc, schar: 10, default: 100);
	total += _Generic(sc, signed char: 20, default: 100);
	total += _Generic(ci, int: 30, default: 100);
	total += _Generic(cf, float: 40, default: 100);
	total += _Generic(cd, double: 50, default: 100);

	return total == 150 ? 42 : total;
}

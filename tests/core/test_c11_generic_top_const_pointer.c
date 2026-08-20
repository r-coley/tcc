int
main(void)
{
	int x = 0;
	int * const p = &x;
	const int * const cp = &x;
	int total = 0;

	total += _Generic(p, int *: 10, default: 100);
	total += _Generic(cp, const int *: 20, default: 100);

	return total == 30 ? 42 : total;
}

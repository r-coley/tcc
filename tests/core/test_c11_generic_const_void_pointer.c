int
main(void)
{
	int x = 0;
	const void *cp = &x;
	int total = 0;

	total += _Generic(cp, const void *: 10, void *: 1, default: 100);

	return total == 10 ? 42 : total;
}

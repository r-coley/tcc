static int
f(void)
{
	return 1;
}

int
main(void)
{
	int (* const fp)(void) = f;
	int total = 0;

	total += _Generic(fp, int (*)(void): 10, default: 100);

	return total == 10 ? 42 : total;
}

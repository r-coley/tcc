int
main(void)
{
	int a[3] = {0, 0, 0};
	int (* volatile p)[3] = &a;
	int total = 0;

	total += _Generic(p, int (*)[3]: 10, default: 100);

	return total == 10 ? 42 : total;
}

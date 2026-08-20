int
f(int n)
{
	int a[8];
	typedef int (*values_t)[n];
	values_t p = (values_t)&a[0];
	values_t q = p + 1;
	int diff = (int)((char *)q - (char *)p);

	return diff == (int)(sizeof(int) * n) ? 42 : diff;
}

int
main(void)
{
	return f(3);
}

static int
f(int n)
{
	typedef int arr_t[n];
	arr_t a;
	arr_t *p = &a;

	a[0] = 10;
	a[1] = 20;
	a[2] = 12;

	if ((int)sizeof(arr_t) != n * (int)sizeof(int))
		return 1;
	if ((int)sizeof(*p) != n * (int)sizeof(int))
		return 2;
	if ((*p)[0] != 10 || (*p)[1] != 20 || (*p)[2] != 12)
		return 3;

	return 42;
}

int
main(void)
{
	return f(3);
}

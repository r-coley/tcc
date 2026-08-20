static int
f(int n)
{
	typedef int arr_t[n];
	arr_t a, b;
	arr_t *pa = &a;
	arr_t *pb = &b;

	a[0] = 1;
	a[1] = 2;
	a[2] = 3;
	b[0] = 4;
	b[1] = 5;
	b[2] = 6;

	if ((int)sizeof(a) != n * (int)sizeof(int))
		return 1;
	if ((int)sizeof(b) != n * (int)sizeof(int))
		return 2;
	if ((int)sizeof(*pa) != n * (int)sizeof(int))
		return 3;
	if ((int)sizeof(*pb) != n * (int)sizeof(int))
		return 4;
	if ((*pa)[0] + (*pa)[1] + (*pa)[2] != 6)
		return 5;
	if ((*pb)[0] + (*pb)[1] + (*pb)[2] != 15)
		return 6;

	return 42;
}

int
main(void)
{
	return f(3);
}

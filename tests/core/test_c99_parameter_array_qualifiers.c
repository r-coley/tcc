static int
sum3(int a[static 3])
{
	return a[0] + a[1] + a[2];
}

static int
head_const(const int a[const 3])
{
	return a[0];
}

static int
combine(int a[restrict static 3], int b[const restrict static 3])
{
	a[0] += b[0];
	a[1] += b[1];
	a[2] += b[2];
	return a[0] + a[1] + a[2];
}

int
main(void)
{
	int a[3] = {10, 20, 12};
	int b[3] = {1, 2, 3};

	if (sum3(a) != 42)
		return 1;
	if (head_const(a) != 10)
		return 2;
	if (combine(a, b) != 48)
		return 3;
	if (a[0] != 11 || a[1] != 22 || a[2] != 15)
		return 4;

	return 42;
}

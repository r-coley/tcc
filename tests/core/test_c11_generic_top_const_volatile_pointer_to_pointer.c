static int
f(int x)
{
	return x;
}

int
main(void)
{
	int x = 0;
	int *p = &x;
	int ** const volatile pp = &p;
	const int *cp = &x;
	const int ** const volatile cpp = &cp;
	int (*fp)(int) = f;
	int (** const volatile fpp)(int) = &fp;
	int total = 0;

	total += _Generic(pp, int **: 10, default: 100);
	total += _Generic(cpp, const int **: 20, default: 100);
	total += _Generic(fpp, int (**)(int): 30, default: 100);

	return total == 60 ? 42 : total;
}

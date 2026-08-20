static int
add2(int a, int b)
{
	return a + b;
}

int
main(void)
{
	int (*varfp)(int, ...) = (int (*)(int, ...))add2;

	return varfp(20, 22);
}

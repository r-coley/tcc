static int
add1(x)
int x;
{
	return x + 1;
}

int
main(void)
{
	int (*oldfp)() = add1;
	int (*protofp)(int) = (int (*)(int))oldfp;

	return protofp(41);
}

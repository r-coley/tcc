char a[];

int
main(void)
{
	if (a[0] != 0)
		return 1;
	a[0] = 42;
	return a[0];
}

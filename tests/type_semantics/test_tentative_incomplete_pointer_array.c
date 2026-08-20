int *a[];

int
main(void)
{
	int value = 42;

	if (a[0] != 0)
		return 1;
	a[0] = &value;
	return *a[0];
}

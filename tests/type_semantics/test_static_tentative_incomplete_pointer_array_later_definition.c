static int *a[];
static int *a[2];

int
main(void)
{
	int value = 42;

	if (a[0] != 0 || a[1] != 0)
		return 1;
	a[1] = &value;
	return *a[1];
}

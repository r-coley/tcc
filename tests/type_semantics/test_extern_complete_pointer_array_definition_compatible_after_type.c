int extern *a[2];
static int x = 40;
static int y = 2;
int *a[] = { &x, &y };

int
main(void)
{
	return *a[0] + *a[1] - 42;
}

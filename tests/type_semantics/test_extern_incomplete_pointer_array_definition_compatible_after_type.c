int extern *a[];
static int x = 40;
static int y = 2;
int *a[2] = { &x, &y };

int
main(void)
{
	return *a[0] + *a[1];
}

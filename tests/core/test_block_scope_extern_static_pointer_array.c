static int a = 19;
static int b = 23;
static int *values[2] = { &a, &b };

int
main(void)
{
	extern int *values[];
	return *values[0] + *values[1];
}

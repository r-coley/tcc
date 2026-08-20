extern int add(int a, int b);
static int sub(int a, int b);

extern int add(int a, int b);
static int sub(int a, int b);

int
add(int a, int b)
{
	return a + b;
}

static int
sub(int a, int b)
{
	return a - b;
}

int
main(void)
{
	return add(40, 4) - sub(5, 3);
}

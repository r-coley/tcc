extern int (*fp)(int);

static int
id(int x)
{
	return x;
}

int (*fp)(int);

int
main(void)
{
	fp = id;
	return fp(7) - 7;
}

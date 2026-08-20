static int
id(int x)
{
	return x;
}

extern int (*fp)(int) = id;

int
main(void)
{
	return fp(7) - 7;
}

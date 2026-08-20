static int
inc(int x)
{
	return x + 1;
}

static int
dec(int x)
{
	return x - 1;
}

int
main(void)
{
	int (*fp)(int) = 1 ? inc : dec;
	return fp(41);
}

int
id_int(int x)
{
	return x;
}

int
main(void)
{
	static int (*fp)(int) = id_int;
	return fp(42) - 42;
}

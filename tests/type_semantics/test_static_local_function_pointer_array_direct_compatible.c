int
id0(int x)
{
	return x;
}

int
id1(int x)
{
	return x + 1;
}

int
main(void)
{
	static int (*table[2])(int) = { [1] = id1, [0] = id0 };
	return table[0](41) + table[1](0) - 42;
}

int
id_int(int x)
{
	return x;
}

int
main(void)
{
	int (*table[1])(int) = { id_int };
	return table[0](42) - 42;
}

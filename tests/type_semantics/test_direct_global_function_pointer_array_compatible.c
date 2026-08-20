int
id_int(int x)
{
	return x;
}

int (*table[])(int) = { id_int };

int
main(void)
{
	return table[0](42) - 42;
}

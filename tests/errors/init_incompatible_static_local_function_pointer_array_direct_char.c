char
id_char(char x)
{
	return x;
}

int
main(void)
{
	static int (*table[1])(int) = { id_char };
	return 0;
}

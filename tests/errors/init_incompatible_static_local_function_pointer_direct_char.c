char
id_char(char x)
{
	return x;
}

int
main(void)
{
	static int (*fp)(int) = id_char;
	return 0;
}

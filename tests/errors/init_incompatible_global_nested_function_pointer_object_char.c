int id_char(char x)
{
	return x;
}

int (*retfn_char(char x))(char)
{
	(void)x;
	return id_char;
}

int (*(*maker)(int))(int) = retfn_char;

int
main(void)
{
	return 0;
}

int id_char(char x)
{
	return x;
}

int (*retfn_char(char x))(char)
{
	(void)x;
	return id_char;
}

int main(void)
{
	static int (*(*maker)(int))(int) = retfn_char;
	return 0;
}

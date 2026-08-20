typedef int (*old_inner)();
typedef int (*char_inner)(char);
typedef old_inner (*old_outer)(void);
typedef char_inner (*char_outer)(void);

int
id_int(int x)
{
	return x;
}

int
id_char(char x)
{
	return x;
}

old_inner
ret_old(void)
{
	return id_int;
}

char_inner
ret_char(void)
{
	return id_char;
}

int
main(void)
{
	return (ret_old == ret_char) ? 1 : 0;
}

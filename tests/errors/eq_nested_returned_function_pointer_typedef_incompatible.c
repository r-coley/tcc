typedef int (*good_inner)(int);
typedef good_inner (*good_outer)(void);
typedef int (*bad_inner)(char);
typedef bad_inner (*bad_outer)(void);

int
inner_int(int x)
{
	return x;
}

int
inner_char(char x)
{
	return x;
}

good_inner
ret_good(void)
{
	return inner_int;
}

bad_inner
ret_bad(void)
{
	return inner_char;
}

int
main(void)
{
	return (ret_good == ret_bad) ? 1 : 0;
}

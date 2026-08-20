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

static good_outer good_fp = ret_good;
static bad_outer bad_fp = ret_bad;

int
main(void)
{
	return (good_fp == bad_fp) ? 1 : 0;
}

typedef int (*good_inner)(int);
typedef good_inner (*good_outer)(void);
typedef int (*bad_inner)(char);
typedef bad_inner (*bad_outer)(void);

int
id1(int x)
{
	return x;
}

int
id2(char x)
{
	return x;
}

good_inner
ret1(void)
{
	return id1;
}

bad_inner
ret2(void)
{
	return id2;
}

static good_outer good_fp = ret1;
static bad_outer bad_fp = ret2;

int
main(void)
{
	return (1 ? good_fp : bad_fp) != 0;
}

typedef int (*good_inner)(int);
typedef good_inner (*good_outer)(void);
typedef int (*bad_inner)(char);
typedef bad_inner (*bad_outer)(void);

int
takes_bad(bad_outer fn)
{
	return fn()(1);
}

int
inner(int x)
{
	return x;
}

good_inner
make_good(void)
{
	return inner;
}

int
main(void)
{
	good_outer fn = make_good;

	return takes_bad(fn);
}

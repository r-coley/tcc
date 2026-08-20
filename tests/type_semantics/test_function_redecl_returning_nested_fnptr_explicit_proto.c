typedef int (*step_fn)(int);
typedef step_fn (*chooser_fn)(int);

static int
inc(int x)
{
	return x + 1;
}

static step_fn
choose(int which)
{
	(void)which;
	return inc;
}

int (*(*maker(void))(int))(int);

chooser_fn maker(void);

chooser_fn
maker(void)
{
	return choose;
}

int
main(void)
{
	return maker()(1)(5) - 6;
}

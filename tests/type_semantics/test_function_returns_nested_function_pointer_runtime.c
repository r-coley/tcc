static int
inc(int x)
{
	return x + 1;
}

static int
(*choose(int which))(int)
{
	(void)which;
	return inc;
}

int
(*(*maker(void))(int))(int)
{
	return choose;
}

int
main(void)
{
	return maker()(1)(5) - 6;
}

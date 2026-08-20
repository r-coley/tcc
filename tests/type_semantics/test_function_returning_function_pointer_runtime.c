static int
inc(int x)
{
	return x + 1;
}

int
(*chooser(int which))(int);

int
(*chooser(int which))(int)
{
	(void)which;
	return inc;
}

int
main(void)
{
	return chooser(0)(41) - 42;
}

static int
inc(int x)
{
	return x + 1;
}

static int
dec(int x)
{
	return x - 1;
}

int
(*pick(int which))(int)
{
	if (which)
		return inc;
	return dec;
}

int
main(void)
{
	return pick(1)(6) - 7;
}

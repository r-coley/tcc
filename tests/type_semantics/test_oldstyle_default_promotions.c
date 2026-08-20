int
sum_small(c, s)
char c;
short s;
{
	return c + s;
}

int
accept_float(f)
float f;
{
	return f == 1.5f ? 1 : 0;
}

int
main(void)
{
	if (sum_small(20, 22) != 42)
		return 1;
	if (!accept_float(1.5f))
		return 2;

	return 42;
}

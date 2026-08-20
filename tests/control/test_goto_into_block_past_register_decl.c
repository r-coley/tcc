int
main(void)
{
	int result = 1;

	goto inside;
	{
		register int x = 1;
		(void)x;
inside:
		result = 42;
	}

	return result;
}

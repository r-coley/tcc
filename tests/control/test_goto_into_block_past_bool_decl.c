int
main(void)
{
	int result = 1;

	goto inside;
	{
		bool x = 1;
		(void)x;
inside:
		result = 42;
	}

	return result;
}

int
main(void)
{
	int result = 1;

	goto inside;
	{
		int _Alignas(16) x = 1;
		(void)x;
inside:
		result = 42;
	}

	return result;
}

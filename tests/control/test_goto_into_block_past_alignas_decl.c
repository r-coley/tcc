int
main(void)
{
	int result = 1;

	goto inside;
	{
		_Alignas(16) int x = 1;
		(void)x;
inside:
		result = 42;
	}

	return result;
}

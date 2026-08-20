int value = 42;

int
main(void)
{
	{
		typedef char value;
		value inner = 2;

		if (sizeof(value) != 1)
			return 10;
		if (inner != 2)
			return 11;
	}

	if (value != 42)
		return 12;

	return value;
}

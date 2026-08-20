typedef int T;

int
main(void)
{
	int value = 1;

	{
		int T = 41;
		value += T;
	}

	return value;
}

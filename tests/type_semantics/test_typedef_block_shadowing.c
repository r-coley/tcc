typedef int T;

int
main(void)
{
	T outer = 1;

	{
		typedef char T;
		T inner = 2;

		if (sizeof(T) != 1)
			return 10;
		if (inner != 2)
			return 11;
	}

	if (sizeof(T) != 4)
		return 12;

	return outer - 1;
}

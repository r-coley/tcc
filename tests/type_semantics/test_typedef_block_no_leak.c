int
main(void)
{
	typedef int T;

	{
		typedef char T;
		T inner = 3;

		if (sizeof(T) != 1)
			return 20;
		if (inner != 3)
			return 21;
	}

	T outer = 4;
	if (sizeof(T) != 4)
		return 22;

	return outer - 4;
}

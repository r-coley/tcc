static int
f(int T)
{
	{
		typedef char T;
		T inner = 2;

		if (sizeof(T) != 1)
			return 10;
		if (inner != 2)
			return 11;
	}

	return T;
}

int
main(void)
{
	return f(42);
}

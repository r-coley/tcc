enum E { A = 1 };

int
main(void)
{
	enum E a = A;

	{
		enum E { B = 2 };
		enum E b = B;

		if (b != B)
			return 10;
	}

	return a - A;
}

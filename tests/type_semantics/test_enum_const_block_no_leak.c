enum { A = 1 };

int
main(void)
{
	int outer = A;

	{
		enum { A = 2 };
		if (A != 2)
			return 10;
	}

	if (A != 1)
		return 11;

	return outer - 1;
}

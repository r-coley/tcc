union U { int x; };

int
main(void)
{
	union U a;
	a.x = 1;

	{
		union U { char c; };
		union U b;
		b.c = 2;

		if (sizeof(union U) != 1)
			return 20;
		if (b.c != 2)
			return 21;
	}

	if (sizeof(union U) != 4)
		return 22;

	return a.x - 1;
}

struct S { int x; };

int
main(void)
{
	struct S a;
	a.x = 1;

	{
		struct S { char c; };
		struct S b;
		b.c = 2;

		if (sizeof(struct S) != 1)
			return 20;
		if (b.c != 2)
			return 21;
	}

	if (sizeof(struct S) != 4)
		return 22;

	return a.x - 1;
}

int
main(void)
{
	{
		struct S { int x; };
	}

	struct S s;
	(void)s;
	return 0;
}

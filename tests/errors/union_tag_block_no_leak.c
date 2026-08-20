int
main(void)
{
	{
		union U { int x; };
	}

	union U u;
	(void)u;
	return 0;
}

int
main(void)
{
	goto inside;

	{
		int n = 4;
		int (*p)[n];
inside:
		return p != 0;
	}
}

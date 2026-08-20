int
main(void)
{
	goto inside;
	{
		int inline helper(void);
inside:
		return 42;
	}
}

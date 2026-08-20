int
main(void)
{
	goto inside;
	{
		int helper(void);
inside:
		return 42;
	}
}

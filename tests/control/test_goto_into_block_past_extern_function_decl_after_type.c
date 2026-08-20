int
main(void)
{
	goto inside;
	{
		int extern helper(void);
inside:
		return 42;
	}
}

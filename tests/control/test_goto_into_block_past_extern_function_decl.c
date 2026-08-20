int
main(void)
{
	goto inside;
	{
		extern int helper(void);
inside:
		return 42;
	}
}

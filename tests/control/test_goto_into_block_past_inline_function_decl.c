int
main(void)
{
	goto inside;
	{
		inline int helper(void);
inside:
		return 42;
	}
}

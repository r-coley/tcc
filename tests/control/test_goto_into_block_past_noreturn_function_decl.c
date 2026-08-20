int
main(void)
{
	goto inside;
	{
		_Noreturn void helper(void);
inside:
		return 42;
	}
}
